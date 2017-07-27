import sys
import os
import bpy

sys.path.append(os.path.abspath("scripts/blender"))
from model_mdl import *

LAYER_MESH = 0

# parse script args
script_args = sys.argv[sys.argv.index('--')+1:]
use_16bit_indices = '--use_16bit_indices' in script_args
output_filename = script_args[script_args.index('--out')+1]



# helper functions

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
		mesh = ob.to_mesh(scene = bpy.context.scene, apply_modifiers = True, settings = 'PREVIEW')
		for mod in ob.modifiers:
			if mod.type == 'ARMATURE':
				mod.show_viewport = True # TODO: remember previous state

		mesh_triangulate(mesh)
		mesh.calc_normals_split()

		my_triangle_batches = []
		unique_list_per_vert = [{} for i in range(len(mesh.vertices))];

		has_vertex_groups = len(ob.vertex_groups) > 0
		if has_vertex_groups:
			assert(ob.parent.type == 'ARMATURE')
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
				while parent is not None and (not (parent in deform_bones)):
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
						while parent is not None and (not (parent in deform_bones)):
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

					my_vertices.append(vertex_data);

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
model.export(output_filename)
