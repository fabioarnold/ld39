import sys
import os
import bpy
import mathutils
from mathutils import Vector
import struct

sys.path.append(os.path.abspath("scripts/blender")) # TODO: find better solution
#sys.path.append("/Users/fabio/Developer/PandoraGame/scripts/blender") # for debugging
from model_mdl import *

"""
notes:
* meshes need to be on first layer
* collision meshes need to be on second layer
* navmeshes need to be on third layer
"""

LAYER_MESH = 0
LAYER_COLLISION = 1

# parse script args
if '--' in sys.argv:
	script_args = sys.argv[sys.argv.index('--')+1:]
	use_16bit_indices = '--use_16bit_indices' in script_args
	output_path = script_args[script_args.index('--out')+1]
	if not os.path.exists(output_path):
		os.makedirs(output_path)
else:
	use_16bit_indices = False
	output_path = os.path.abspath("build/data")



# helper functions

def veckey3d(v):
	return round(v.x,3), round(v.y,3), round(v.z,3)

# since mathutils.geometry.intersect_line_plane sucks
def intersect_segment_plane(s0, s1, pn, pd):
	d0 = pn.dot(s0)-pd
	d1 = pn.dot(s1)-pd
	if d0*d1 > -1.0e-6: # equal sign and parallel check
		return None
	t = d0/(d0-d1)
	return s0 + t * (s1-s0)

def intersectAABBVsLine(bmin, bmax, v0, v1):
	if v0.x < bmin.x and v1.x < bmin.x:
		return False
	if v0.x > bmax.x and v1.x > bmax.x:
		return False
	if v0.y < bmin.y and v1.y < bmin.y:
		return False
	if v0.y > bmax.y and v1.y > bmax.y:
		return False

	dx = v1.x - v0.x
	dy = v1.y - v0.y
	if abs(dx) < 1e-6 or abs(dy) < 1e-6:
		return True # nearly parallel

	tminx = (bmin.x - v0.x) / dx
	tmaxx = (bmax.x - v0.x) / dx
	tminy = (bmin.y - v0.y) / dy
	tmaxy = (bmax.y - v0.y) / dy

	t0x = min(tminx, tmaxx)
	t1x = max(tminx, tmaxx)
	t0y = min(tminy, tmaxy)
	t1y = max(tminy, tmaxy)

	return t0x < t1y and t0y < t1x # segments need to overlap

def mesh_triangulate(me):
	import bmesh
	bm = bmesh.new()
	bm.from_mesh(me)
	bmesh.ops.triangulate(bm, faces=bm.faces)
	bm.to_mesh(me)
	bm.free()



# extract scene data
def getSceneData():
	# extract image paths from cycles materials
	my_materials = [] # indexed by MDLTriangleBatch
	my_materials_map = {} # maps name to index
	for mat in bpy.data.materials:
		if mat.use_nodes:
			for node in mat.node_tree.nodes:
				if node.bl_idname == 'ShaderNodeTexImage':
					image = node.image
					my_materials_map[mat.name] = len(my_materials)
					my_materials.append(MDLMaterial(os.path.splitext(image.filepath)[0]))

	mdl_bones = []
	mdl_actions = []
	my_meshes = []
	my_meshes_map = {}
	my_vertices = []
	my_vertex_arrays = []
	for ob in bpy.data.objects:
		if not ob.layers[LAYER_MESH]:
			continue
		if ob.type != 'MESH':
			continue

		# turn off preview of armature modifier so we can export the bind pose
		for mod in ob.modifiers:
			if mod.type == 'ARMATURE':
				mod.show_viewport = False
		mesh = ob.to_mesh(scene=bpy.context.scene, apply_modifiers=True, settings='PREVIEW')
		for mod in ob.modifiers:
			if mod.type == 'ARMATURE':
				mod.show_viewport = True # TODO: remember previous state

		mesh_triangulate(mesh)
		mesh.calc_normals_split()

		my_triangle_batches = []
		unique_list_per_vert = [{} for i in range(len(mesh.vertices))]

		has_vertex_groups = len(ob.vertex_groups) > 0
		if has_vertex_groups:
			assert ob.parent.type == 'ARMATURE'
			armature_ob = ob.parent
			armature = armature_ob.data
			vertex_groups = ob.vertex_groups
			vertex_group_names = [vg.name for vg in vertex_groups]

			bone_names = [bone.name for bone in armature.bones]
			deform_bones = []
			for bone in armature.bones:
				if bone.name in vertex_group_names:
					deform_bones.append(bone)

			bone_name_index_map = {}
			for i, bone in enumerate(deform_bones):
				bone_name_index_map[bone.name] = i

			for bone in deform_bones:
				matrix_local = bone.matrix_local
				parent = bone.parent
				while parent is not None and (not parent in deform_bones):
					parent = parent.parent

				if parent is None:
					parent_index = 255
				else:
					parent_index = bone_name_index_map[parent.name]

				inv_bind_mat = matrix_local.inverted()
				mdl_bones.append(MDLBone(bone.name, parent_index, inv_bind_mat))

			# get action data
			for action in bpy.data.actions:
				action_bone_names = [] # names of bones affected by action
				for fcurve in action.fcurves:
					try:
						prop = armature_ob.path_resolve(fcurve.data_path, False)
					except:
						prop = None

					if prop is not None:
						if isinstance(prop.data, bpy.types.PoseBone):
							action_bone_names.append(prop.data.name)
							# calculate the matrices

				if set(action_bone_names).intersection(set(bone_names)): # action is relevant
					frame_count = int(action.frame_range[1]) - int(action.frame_range[0]) + 1
					mdl_tracks = []
					for bone in deform_bones:
						is_animated = False
						if bone.name in action_bone_names:
							is_animated = True
						# check if non deforming parent is animated
						parent = bone.parent
						while parent is not None and (not parent in deform_bones):
							if parent.name in action_bone_names:
								is_animated = True
							parent = parent.parent
						is_animated = True # TODO: temp fix
						if is_animated:
							mdl_tracks.append(MDLActionTrack(bone_name_index_map[bone.name], []))

					armature_ob.animation_data.action = action # make action active
					for frame in range(int(action.frame_range[0]), int(action.frame_range[1]) + 1):
						bpy.context.scene.frame_set(frame)
						for mdl_track in mdl_tracks:
							mdl_bone = mdl_bones[mdl_track.bone_index]
							pose_bone = armature_ob.pose.bones[mdl_bone.name]
							matrix = pose_bone.matrix
							if mdl_bone.hasParent():
								pose_bone_parent = armature_ob.pose.bones[mdl_bones[mdl_bone.parent_index].name]
								matrix = pose_bone_parent.matrix.inverted() * matrix
							loc, rot, scale = matrix.decompose()
							mdl_track.bone_poses.append(MDLBoneTransform(loc, rot, scale[0]))

					mdl_actions.append(MDLAction(action.name, frame_count, mdl_tracks))

		prev_mat_index = -1
		my_triangle_batch = None
		has_uvs = len(mesh.uv_layers)
		for face in sorted(mesh.polygons, key=lambda face: face.material_index):
			if face.material_index != prev_mat_index:
				prev_mat_index = face.material_index
				try:
					mat_index = my_materials_map[mesh.materials[face.material_index].name]
				except:
					mat_index = -1
				my_triangle_batch = MDLTriangleBatch(mat_index)
				my_triangle_batches.append(my_triangle_batch)

			indices = [0, 0, 0] # init
			for i, vert_index in enumerate(face.vertices):
				loop_index = face.loop_indices[i]

				pos = mesh.vertices[vert_index].co
				normal = mesh.loops[loop_index].normal
				if has_uvs:
					try:
						texcoord = Vector(mesh.uv_layers.active.data[loop_index].uv)
						texcoord.y = 1.0 - texcoord.y # flip y coordinate
					except:
						texcoord = Vector([0.0, 0.0])

				# duplicate vertices if they are unique
				if has_uvs:
					vert_key = round(normal.x, 6), round(normal.y, 6), round(normal.z, 6), round(texcoord.x, 6), round(texcoord.y, 6)
				else:
					vert_key = round(normal.x, 6), round(normal.y, 6), round(normal.z, 6)
				unique_list = unique_list_per_vert[vert_index]
				try:
					indices[i] = unique_list[vert_key]
				except:
					unique_list[vert_key] = len(my_vertices)
					indices[i] = len(my_vertices)

					vertex_data = list(pos)
					vertex_data.extend(normal)
					if has_uvs:
						vertex_data.extend(texcoord)

					if has_vertex_groups:
						bone_indices = [0 for i in range(4)]
						bone_weights = [0.0 for i in range(3)]

						# sort so least important bones get cut off
						sorted_groups = sorted(mesh.vertices[vert_index].groups, key = lambda group: group.weight, reverse = True)
						total_weight = 0.0
						for i, group in enumerate(sorted_groups[:4]):
							bone_indices[i] = bone_name_index_map[vertex_groups[group.group].name]
							total_weight += group.weight
						# normalize weights
						for i, group in enumerate(sorted_groups[:3]):
							bone_weights[i] = group.weight / total_weight

						vertex_data.extend(bone_indices) # bone_index
						vertex_data.extend(bone_weights) # bone_influence

					my_vertices.append(vertex_data)

			my_triangle_batch.indices.extend(indices)

		my_meshes_map[ob.data.name] = len(my_meshes) # TODO: use unique meshes
		my_meshes.append(MDLMesh(0, my_triangle_batches))


	# scene graph
	# TODO: parent child relationship support
	my_nodes = []
	for ob in bpy.data.objects:
		if ob.layers[LAYER_MESH] and ob.type == 'MESH':
			mesh_index = my_meshes_map[ob.data.name]
			loc, rot, scale = ob.matrix_world.decompose()
			transform = MDLTransform(loc, rot, scale)
			bbox_corners = [ob.matrix_world * Vector(corner) for corner in ob.bound_box]
			aabb = MDLAABB(bbox_corners)
			my_nodes.append(MDLNode(mesh_index, transform, aabb))



	attribs = []
	attribs.append(MDLVertexAttrib('VAT_POSITION', 3, 'DT_FLOAT'))
	attribs.append(MDLVertexAttrib('VAT_NORMAL', 3, 'DT_FLOAT'))
	if has_uvs:
		attribs.append(MDLVertexAttrib('VAT_TEXCOORD0', 2, 'DT_FLOAT'))
	if has_vertex_groups:
		attribs.append(MDLVertexAttrib('VAT_BONE_INDEX', 4, 'DT_UNSIGNED_BYTE'))
		attribs.append(MDLVertexAttrib('VAT_BONE_WEIGHT', 3, 'DT_FLOAT'))
	vertex_format = MDLVertexFormat(attribs)
	my_vertex_arrays.append(MDLVertexArray(vertex_format, my_vertices))

	return my_nodes, my_materials, mdl_bones, mdl_actions, my_vertex_arrays, my_meshes



# export scene meshes

my_nodes, my_materials, mdl_bones, mdl_actions, my_vertex_arrays, my_meshes = getSceneData()
model = MDLModel()
if my_nodes:
	model.addChunk(MDLNodeChunk(my_nodes))
if my_materials:
	model.addChunk(MDLMaterialChunk(my_materials))
if mdl_bones:
	model.addChunk(MDLSkeletonChunk(mdl_bones))
if mdl_actions:
	model.addChunk(MDLActionChunk(mdl_actions))
model.addChunk(MDLVertexChunk(my_vertex_arrays))
model.addChunk(MDLTriangleChunk(my_meshes))
model.export(output_path + "/model.mdl")



# export static collision data

class ColMesh2D:
	bounds_min = Vector(( float("inf"),  float("inf")))
	bounds_max = Vector((-float("inf"), -float("inf")))

	vert_indices = {} # find duplicates
	#edge_set = set() # find duplicate edges
	verts = []
	edges = []
	vert_edges = [] # incident edges of a vertex (indexed by vert index)

	spawnpoints = []

	navgrid = [[]] # empty

	def updateBounds(self, v):
		self.bounds_min.x = min(v.x, self.bounds_min.x)
		self.bounds_min.y = min(v.y, self.bounds_min.y)
		self.bounds_max.x = max(v.x, self.bounds_max.x)
		self.bounds_max.y = max(v.y, self.bounds_max.y)

	def getIndex(self, v):
		self.updateBounds(v)
		k = veckey3d(v)
		i = self.vert_indices.get(k)
		if i is None:
			i = len(self.verts)
			self.vert_indices[k] = i
			self.verts.append(v)
			self.vert_edges.append([]) # alloc list
		return i

	def getEdgeNormal(self, e):
		v0 = self.verts[e[0]]
		v1 = self.verts[e[1]]
		return Vector((v1.y-v0.y, v0.x-v1.x)).normalized()

	def addEdge(self, e0, e1):
		i0 = self.getIndex(e0)
		i1 = self.getIndex(e1)
		if i0 == i1:
			return
		#if not edge in self.edge_set:

		# check if i0 is a collinear point
		for incident_edge in self.vert_edges[i0][:]:
			i2 = incident_edge[0] if i0 != incident_edge[0] else incident_edge[1]
			area = mathutils.geometry.area_tri(e0, e1, self.verts[i2])
			if area < 1e-4:
				# collinear -> remove i0
				#print("collinear edge found %f" % area)
				self.edges.remove(incident_edge)
				self.vert_edges[i0].remove(incident_edge)
				self.vert_edges[i2].remove(incident_edge)
				i0 = i2
				e0 = self.verts[i2]
				break

		# check if i1 is a collinear point
		for incident_edge in self.vert_edges[i1][:]:
			i2 = incident_edge[0] if i1 != incident_edge[0] else incident_edge[1]
			area = mathutils.geometry.area_tri(e0, e1, self.verts[i2])
			if area < 1e-4:
				# collinear -> remove i1
				#print("collinear edge found %f" % area)
				self.edges.remove(incident_edge)
				self.vert_edges[i1].remove(incident_edge)
				self.vert_edges[i2].remove(incident_edge)
				i1 = i2
				e1 = self.verts[i2]
				break

		edge = (i0, i1) #if i0<i1 else (i1,i0)
		self.edges.append(edge)
		# add incident edges
		self.vert_edges[i0].append(edge)
		self.vert_edges[i1].append(edge)

	def rebuildVertexList(self):
		verts_copy = self.verts[:]
		edges_copy = self.edges[:]
		self.verts = []
		self.edges = []
		self.vert_indices = {}
		self.vert_edges = []
		for e in edges_copy:
			i0 = self.getIndex(verts_copy[e[0]])
			i1 = self.getIndex(verts_copy[e[1]])
			edge = (i0, i1) #if i0<i1 else (i1,i0)
			self.edges.append(edge)
			# add incident edges
			self.vert_edges[i0].append(edge)
			self.vert_edges[i1].append(edge)

	def toGridPoint(self, p):
		return (int((p.x - self.bounds_min.x) / self.navgrid_tile_size), int((p.y - self.bounds_min.y) / self.navgrid_tile_size))

	# also deletes collision edges which the player could never encounter
	def buildNavGrid(self):
		# find grid points which intersect collision edges
		# floodfill starting at spawn points
		# mark edges that still intersect grid points at floodfill perimeter
		# this is the relevant subset of all collision edges
		# outset these collision edges by the tank radius resulting in the navigation polygon

		# dilate edges using maxnorm by tank radius
		dilated_verts = []
		TANK_RADIUS = 1.337
		for i, v in enumerate(self.verts):
			v_min = Vector((0.0, 0.0))
			v_max = Vector((0.0, 0.0))
			for e in self.vert_edges[i]:
				normal = self.getEdgeNormal(e)
				v_min.x = min(v_min.x, normal.x)
				v_min.y = min(v_min.y, normal.y)
				v_max.x = max(v_max.x, normal.x)
				v_max.y = max(v_max.y, normal.y)
			# TODO: this is actually a polygon, insert vertices as necessary
			v2d = (v_min + v_max) * TANK_RADIUS
			dilated_verts.append(v + Vector((v2d.x, v2d.y, 0.0)))

		# find all edge gridpoints
		tile_size = self.navgrid_tile_size
		off_x = self.bounds_min.x
		off_y = self.bounds_min.y
		w = int((self.bounds_max.x - self.bounds_min.x) / tile_size + 0.5)
		h = int((self.bounds_max.y - self.bounds_min.y) / tile_size + 0.5)
		self.navgrid = [[0 for x in range(w)] for y in range(h)]
		edgegrid = [[0 for x in range(w)] for y in range(h)]

#		edge_gridpointlist = [[] for e in self.edges]
		for y in range(h):
			for x in range(w):
				cmin = Vector((off_x + x*tile_size, off_y + y*tile_size))
				cmax = Vector((cmin.x + tile_size, cmin.y + tile_size))
				for i, e in enumerate(self.edges):
					if intersectAABBVsLine(cmin, cmax, dilated_verts[e[0]], dilated_verts[e[1]]):
						edgegrid[y][x] = 1
						break
#						edge_gridpointlist[i].append((x, y))

		# do floodfill starting at the first spawnpoint
		stack = [self.toGridPoint(self.spawnpoints[0])]
		while len(stack) > 0:
			p = stack.pop()
			if p[0] < 0 or p[0] >= w or p[1] < 0 or p[1] >= h:
				continue # out of bounds
			if edgegrid[p[1]][p[0]] == 1:
				self.navgrid[p[1]][p[0]] = 2 # mark edge and stop
			if self.navgrid[p[1]][p[0]] != 0:
				continue # already seen
			self.navgrid[p[1]][p[0]] = 1 # mark as walkable
			# spawn children
			stack.append((p[0], p[1]+1)) # north
			stack.append((p[0]+1, p[1]+1)) # northwest
			stack.append((p[0]+1, p[1])) # west
			stack.append((p[0]+1, p[1]-1)) # southwest
			stack.append((p[0], p[1]-1)) # south
			stack.append((p[0]-1, p[1]-1)) # southeast
			stack.append((p[0]-1, p[1])) # east
			stack.append((p[0]-1, p[1]+1)) # northeast

		# check if spawnpoints are reachable
		for sp in self.spawnpoints:
			gp = self.toGridPoint(sp)
			if self.navgrid[gp[1]][gp[0]] != 1:
				print("nav: spawnpoint is not reachable at %f %f" % (sp.x, sp.y))

		# remove any edges which don't intersect a grid edge
#		verts_copy = self.verts[:]
#		edges_copy = self.edges[:]
#		self.vert_indices = {}
#		self.vert_edges = []
#		self.verts = []
#		self.edges = []
#		for i, e in enumerate(edges_copy):
#			for x, y in edge_gridpointlist[i]:
#				if self.navgrid[y][x] == 2:
#					self.addEdge(verts_copy[e[0]], verts_copy[e[1]])
#					break

	def createFrom(scene, plane_no, plane_d):
		cm = ColMesh2D()
		cm.navgrid_tile_size = 1.0 # ~ radius of tank

		for ob in scene.objects:
			if ob.type == 'EMPTY':
				if ob.name.startswith("spawn"):
					#index = int(ob.name[len("spawn"):])
					cm.spawnpoints.append(Vector((ob.location.x, ob.location.y)))
			if ob.type != 'MESH':
				continue
			me = ob.to_mesh(scene, apply_modifiers=True, settings='PREVIEW', calc_tessface=False)
			me.transform(ob.matrix_world)

			for f in me.polygons:
				isects = []
				for i, v0 in enumerate(f.vertices):
					v1 = f.vertices[(i+1) % len(f.vertices)]
					e0 = me.vertices[v0].co.copy()
					e1 = me.vertices[v1].co.copy()

					isect = intersect_segment_plane(e0, e1, plane_no, plane_d)
					if isect is not None:
						isects.append(isect)

				if len(isects) == 2:
					e0 = isects[0]
					e1 = isects[1]
					# edge normal points to right (outside), orient it like the face normal
					edge_normal = Vector((e1.y-e0.y, e0.x-e1.x))
					if edge_normal.dot(Vector((f.normal.x, f.normal.y))) > 0.0:
						cm.addEdge(e0, e1)
					else:
						cm.addEdge(e1, e0)
				elif len(isects) > 0:
					print(ob.name)

		assert len(cm.spawnpoints) > 0

		# rebuild clean vertex list (because of removal of collinear points)
		cm.rebuildVertexList()

		# based on edges render blocked tiles to grid
		cm.buildNavGrid()

		return cm

	def write(self, filepath):
		file = open(filepath, "wb")

		print("vertices: %d" % len(self.verts))
		print("edges: %d" % len(self.edges))

		# bounds
		file.write(struct.pack("4f", self.bounds_min.x, self.bounds_min.y, self.bounds_max.x, self.bounds_max.y))

		# verts and edges
		file.write(struct.pack("2I", len(self.verts), len(self.edges)))
		for v in self.verts:
			file.write(struct.pack("2f", v.x, v.y))
		for e in self.edges:
			file.write(struct.pack("2I", *e))

		# four spawnpoints
		file.write(struct.pack("I", len(self.spawnpoints)))
		for sp in self.spawnpoints:
			file.write(struct.pack("2f", *sp))

		# navgrid
		w = len(self.navgrid[0])
		h = len(self.navgrid)
		file.write(struct.pack("2I1f", w, h, self.navgrid_tile_size))
		for y in range(h):
			file.write(struct.pack("%dB" % w, *self.navgrid[y]))

		file.flush()
		file.close()

	def printCode(self):
		# print c++ code
		for i,v in enumerate(self.verts):
			print("points[%d] = v2(%.3f,%.3f);" % (i,v.x,v.y))
		for i,e in enumerate(self.edges):
			print("edges[%d].pi[0] = %d;" % (i,e[0]))
			print("edges[%d].pi[1] = %d;" % (i,e[1]))

		print("point_count = %d;" % len(self.verts))
		print("edge_count = %d;" % len(self.edges))

# export meta information
cm = ColMesh2D.createFrom(bpy.context.scene, Vector((0.0, 0.0, 1.0)), 0.5)
cm.write(output_path + "/meta.bin")



class COLTriangle:
	def __init__(self, positions):
		self.positions = positions
		# derived data
		self.normal = (positions[1]-positions[0]).cross(positions[2]-positions[0])
		self.normal.normalize()
		self.normal_d = self.normal.dot(positions[0])
		# calc aabb
		self.bmin = positions[0].copy()
		self.bmax = positions[0].copy()
		for p in positions[1:]:
			for i in range(3):
				if p[i] < self.bmin[i]:
					self.bmin[i] = p[i]
				if p[i] > self.bmax[i]:
					self.bmax[i] = p[i]

	def getBoundsAxisCenter(self, axis):
		return 0.5 * (self.bmin[axis]+self.bmax[axis])

	def getSize():
		return struct.calcsize("9f3f1f")

	def write(self, file):
		packed_data = b""
		for p in self.positions:
			packed_data += struct.pack("3f", p.x, p.y, p.z)
		packed_data += struct.pack("4f", self.normal.x, self.normal.y, self.normal.z, self.normal_d)
		file.write(packed_data)

class COLNode:
	def __init__(self, triangles, offset):
		self.triangles = triangles # uh... what waste of memory (stupid python is bad at in-place stuff)
		self.offset = offset # positive -> is leaf and offset represents triangle offset
		# calc aabb
		self.bmin = triangles[0].bmin.copy()
		self.bmax = triangles[0].bmax.copy()
		for t in triangles[1:]:
			for i in range(3):
				if t.bmin[i] < self.bmin[i]:
					self.bmin[i] = t.bmin[i]
				if t.bmax[i] > self.bmax[i]:
					self.bmax[i] = t.bmax[i]

	def isLeaf(self):
		return self.offset >= 0

	# builds a simple axis aligned kd-tree
	def buildTree(triangles, triangles_per_leaf):
		nodes = []
		COLNode.subdivide(triangles, triangles_per_leaf, 0, nodes) # each subdivision will add another node
		return nodes

	# recursive
	def subdivide(triangles, triangles_per_leaf, offset, nodes):
		node = COLNode(triangles, offset)
		nodes.append(node)

		if len(triangles) > triangles_per_leaf: # split
			# find longest axis
			axis = 0
			if node.bmax[1]-node.bmin[1] > node.bmax[0]-node.bmin[0]:
				axis = 1
			if node.bmax[2]-node.bmax[2] > node.bmax[axis]-node.bmin[axis]:
				axis = 2
			# sort triangles along axis
			#print("sorting triangles")
			node.triangles.sort(key=lambda tri: tri.getBoundsAxisCenter(axis))
			isplit = int(len(triangles)/2) # balanced split
			prev_node_count = len(nodes)
			COLNode.subdivide(triangles[:isplit], triangles_per_leaf, offset, nodes)
			COLNode.subdivide(triangles[isplit:], triangles_per_leaf, offset+isplit, nodes)
			node.offset = prev_node_count - len(nodes) # negative -> is node with -offset children

	def getSize():
		return struct.calcsize("3f3fiI")

	def write(self, file):
		packed_data  = struct.pack("3f", self.bmin.x, self.bmin.y, self.bmin.z)
		packed_data += struct.pack("3f", self.bmax.x, self.bmax.y, self.bmax.z)
		packed_data += struct.pack("iI", self.offset, len(self.triangles))
		file.write(packed_data)

class COLMesh:
	def __init__(self, triangles):
		# build acceleration structure
		self.nodes = COLNode.buildTree(triangles, 1) # TODO: tune this number
		# because we can't do in-place sort we have to get the triangle order from the node tree
		self.triangles = []

		for node in self.nodes:
			if node.isLeaf():
				self.triangles.extend(node.triangles)

	def write(self, filepath):
		file = open(filepath, "wb")

		magic_num = b"COL1"
		version = 1

		# nodes first
		node_count = len(self.nodes)
		triangle_count = len(self.triangles)

		header_format = "4s4I" # magic, version, file_size, node_count, triangle_count
		header_size = struct.calcsize(header_format)
		file_size = header_size + node_count*COLNode.getSize() + triangle_count*COLTriangle.getSize()

		# write header
		file.write(struct.pack(header_format, magic_num, version, file_size, node_count, triangle_count))

		for node in self.nodes:
			node.write(file)
		for triangle in self.triangles:
			triangle.write(file)

		file.flush()
		file.close()


# export 3D collision mesh
#my_col_triangles = []
#for ob in bpy.data.objects:
#	if ob.layers[LAYER_COLLISION]: # object is on first layer
#		# TODO: other predicates to avoid dynamic objects and stuff
#		if ob.type == 'MESH':
#			mesh = ob.data.copy()
#			mesh_triangulate(mesh)
#			mesh.transform(ob.matrix_world)
#			for face in mesh.polygons:
#				positions = []
#				for vert_index in face.vertices:
#					pos = mesh.vertices[vert_index].co.copy()
#					positions.append(pos)
#				my_col_triangles.append(COLTriangle(positions))

#my_col_mesh = COLMesh(my_col_triangles)
#my_col_mesh.write(output_path + "/static.col")



# bpy.ops.mesh.navmesh_make()

# export navmesh
#scene = bpy.context.scene
#scene.layers[LAYER_NAVIGATION] = True
#navmesh = None
#for ob in bpy.data.objects:
#	if ob.layers[LAYER_NAVIGATION]: # object is on second layer
#		if ob.name == "navmesh":
#			scene.objects.active = ob
#			bpy.ops.object.mode_set(mode='EDIT', toggle=False)

#			bpy.ops.mesh.reveal()
#			bpy.ops.mesh.select_all(action='SELECT')
#			bpy.ops.mesh.remove_doubles(threshold=0.001)
#			bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

#			navmesh = ob.data
#			mesh_triangulate(navmesh)

# helper func
def edgeHash(v0, v1, vertex_count):
	if v0 < v1:
		return v0*vertex_count + v1
	elif v1 < v0:
		return v1*vertex_count + v0

#if navmesh is not None:
#	neighbors = []
#	unique_edges = {}
#	for it, p in enumerate(navmesh.polygons):
#		neighbors.append([-1, -1, -1])
#		for iv in range(3):
#			iv1 = (iv+1) % 3
#			edge_index = edgeHash(p.vertices[iv], p.vertices[iv1], len(navmesh.vertices))
#			if edge_index in unique_edges:
#				neighbors[int(unique_edges[edge_index]/3)][int(unique_edges[edge_index]%3)] = it
#				neighbors[it][iv] = int(unique_edges[edge_index]/3)
#			else:
#				unique_edges[edge_index] = 3*it + iv

#	filepath = output_path + "/navigation.navmesh"
#	file = open(filepath, "wb")

#	file.write(struct.pack("2I", len(navmesh.vertices), len(navmesh.polygons)))

#	for v in navmesh.vertices:
#		file.write(struct.pack("3f", *v.co))
#	for it, p in enumerate(navmesh.polygons):
#		file.write(struct.pack("3I", *p.vertices))
#		file.write(struct.pack("3i", *neighbors[it]))
#		#file.write(struct.pack("3f", 0.0, 0.0, 0.0)) # empty normal
#		file.write(struct.pack("3f", *p.normal)) # normal

#	file.flush()
#	file.close()
