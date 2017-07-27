import bpy
import mathutils

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

def createMesh(verts, edges):
	# create mesh
	me = bpy.data.meshes.new('me_col_lines')
	ob = bpy.data.objects.new('ob_col_lines', me)

	scn = bpy.context.scene
	scn.objects.link(ob)
	scn.objects.active = ob
	ob.select = True

	me.from_pydata(verts, edges, [])
	me.update()

class ColMesh2D:
	vert_indices = {} # find duplicates
	verts = []
	edges = []

	def getIndex(self, v):
		k = veckey3d(v)
		i = self.vert_indices.get(k)
		if i is None:
			i = len(self.verts)
			self.vert_indices[k] = i
			self.verts.append(v)
		return i

	def addEdge(self, e0, e1):
		i0 = self.getIndex(e0)
		i1 = self.getIndex(e1)
		if i0 == i1:
			return
		self.edges.append((i0, i1))

	def createFrom(objects, plane_no, plane_d):
		cm = ColMesh2D()

		for ob in objects:
			if ob.type != 'MESH':
				continue
			me = ob.to_mesh(bpy.context.scene, apply_modifiers=True, settings='PREVIEW', calc_tessface=False)
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
					edge_normal = mathutils.Vector((e1.y-e0.y, e0.x-e1.x))
					if edge_normal.dot(mathutils.Vector((f.normal.x, f.normal.y))) > 0.0:
						cm.addEdge(e0, e1)
					else:
						cm.addEdge(e1, e0)
				elif len(isects) > 0:
					print(ob.name)

		createMesh(cm.verts, cm.edges)

	def printCode(self):
		# print c++ code
		for i,v in enumerate(self.verts):
			print("points[%d] = v2(%.3f,%.3f);" % (i,v.x,v.y))
		for i,e in enumerate(self.edges):
			print("edges[%d].pi[0] = %d;" % (i,e[0]))
			print("edges[%d].pi[1] = %d;" % (i,e[1]))

		print("point_count = %d;" % len(self.verts))
		print("edge_count = %d;" % len(self.edges))


 
class OBJECT_PT_pingpong(bpy.types.Panel):
	bl_label = "Collision Mesh"
	bl_space_type = "PROPERTIES"
	bl_region_type = "WINDOW"
	bl_context = "object"
 
	def draw_header(self, context):
		layout = self.layout
		layout.label(text="", icon="PHYSICS")
 
	def draw(self, context):
		layout = self.layout
		layout.operator("object.make_col_mesh")

 
class OBJECT_OT_pingpong(bpy.types.Operator):
	bl_label = "Make Collision Mesh Operator"
	bl_idname = "object.make_col_mesh"
	bl_description = "Generate Collision Mesh"
	# intersect meshes with this plane
	plane_no = bpy.props.FloatVectorProperty(name="Plane normal", default=(0.0, 0.0, 1.0))
	plane_d = bpy.props.FloatProperty(name="Plane d", default=0.5)
 
	def execute(self, context):
		ColMesh2D.createFrom(context.scene.objects, mathutils.Vector(self.plane_no), self.plane_d)
		self.report({'INFO'}, "Making collision mesh")
		return {'FINISHED'}
 
def register():
	bpy.utils.register_module(__name__)
 
def unregister():
	bpy.utils.unregister_module(__name__)
 
if __name__ == "__main__":
	register()
