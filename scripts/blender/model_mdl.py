import sys
from mathutils import Vector
import struct

# MDL type defs
class MDLMaterial:
	def __init__(self, filepath):
		# store filepath relative to the textures folder
		textures_basepath = 'textures/'
		index = filepath.find(textures_basepath);
		self.filepath = filepath[index+len(textures_basepath):]

class MDLTransform: # only used by MDLNode
	def __init__(self, location, rotation, scale):
		self.location = location
		self.rotation = rotation
		self.scale = scale

class MDLAABB:
	def __init__(self, vertices):
		self.min = Vector(3*[sys.float_info.max])
		self.max = Vector(3*[-sys.float_info.max])
		for v in vertices:
			for i in range(3):
				if self.min[i] > v[i]:
					self.min[i] = v[i]
				if self.max[i] < v[i]:
					self.max[i] = v[i]

class MDLNode:
	def __init__(self, mesh_index, transform, aabb):
		self.mesh_index = mesh_index
		self.transform = transform
		self.aabb = aabb

class MDLBone:
	def __init__(self, name, parent_index, inv_bind_mat):
		self.name = name
		self.parent_index = parent_index # 255 == no parent
		self.inv_bind_mat = inv_bind_mat

	def hasParent(self):
		return self.parent_index != 255

	def getSize():
		return struct.calcsize("14sxB16f")

	def write(self, file):
		packed_mat = b""
		for col in range(4):
			for row in range(4):
				packed_mat += struct.pack("f", self.inv_bind_mat[row][col])
		file.write(struct.pack("14sxB", self.name.encode("utf-8"), self.parent_index) + packed_mat)

class MDLBoneTransform:
	def __init__(self, translation, rotation, scale):
		self.translation = translation
		self.rotation = rotation
		self.scale = scale # single component

	def getSize():
		return struct.calcsize("8f")

	def write(self, file):
		file.write(struct.pack("8f", self.translation.x, self.translation.y, self.translation.z, self.rotation.x, self.rotation.y, self.rotation.z, self.rotation.w, self.scale))

class MDLActionTrack:
	def __init__(self, bone_index, bone_poses):
		self.bone_index = bone_index
		self.bone_poses = bone_poses # len(bone_poses) has to match frame_count of action

	def getSize(self):
		return struct.calcsize("1I") + MDLBoneTransform.getSize() * len(self.bone_poses)

	def write(self, file):
		file.write(struct.pack("1I", self.bone_index))
		for bone_pose in self.bone_poses:
			bone_pose.write(file)

class MDLAction:
	def __init__(self, name, frame_count, tracks):
		self.name = name
		self.frame_count = frame_count
		self.tracks = tracks

	def getSize(self):
		size = struct.calcsize("15sx2I")
		for track in self.tracks:
			size += track.getSize()
		return size

	def write(self, file):
		file.write(struct.pack("15sx2I", self.name.encode("utf-8"), self.frame_count, len(self.tracks)))
		for track in self.tracks:
			assert(len(track.bone_poses) == self.frame_count)
			track.write(file)

DataType = {
	'DT_FLOAT' : 0,
	'DT_UNSIGNED_BYTE' : 1,
	'DT_UNSIGNED_SHORT' : 2,
	'DT_UNSIGNED_INT': 3
}
DataTypeFormatChar = ("f", "B", "H", "I") # struct.pack

VertexAttribType = {
	'VAT_POSITION' : 0,
	'VAT_NORMAL' : 1,
	'VAT_TANGENT' : 2,
	'VAT_TEXCOORD0' : 3,
	'VAT_TEXCOORD1' : 4,
	'VAT_COLOR' : 5,
	'VAT_BONE_INDEX' : 6,
	'VAT_BONE_WEIGHT' : 7
}

class MDLVertexAttrib:
	def __init__(self, type, components_count, component_type):
		self.type = type
		self.components_count = components_count
		self.component_type = component_type

	def getFormatString(self):
		return str(self.components_count) + DataTypeFormatChar[DataType[self.component_type]]

	def getTuple(self):
		return (self.components_count, DataType[self.component_type])

class MDLVertexFormat:
	def __init__(self, attribs):
		self.attribs = 8*[(0, 0)] # empty (count, type) tuples
		assert(len(attribs) <= 8)
		self.vertex_format_string = ""
		for attrib in attribs:
			assert(attrib.components_count != 0)
			self.vertex_format_string += attrib.getFormatString()
			self.attribs[VertexAttribType[attrib.type]] = attrib.getTuple()

	def getVertexFormatString(self):
		return self.vertex_format_string

	def getVertexSize(self):
		return struct.calcsize(self.vertex_format_string)

	def getSize(self):
		return struct.calcsize("16B")

	def write(self, file):
		file.write(struct.pack("16B", *[i for t in self.attribs for i in t]))
		# write dummy vertex format (pos, nor, uv)
		#file.write(struct.pack("16B", 3,0, 3,0, 0,0, 2,0, 0,0, 0,0, 0,0, 0,0))

class MDLVertexArray:
	def __init__(self, vertex_format, vertex_data):
		self.format = vertex_format
		self.vertex_data = vertex_data

	def getSize(self):
		return self.format.getSize() + 4 + self.format.getVertexSize()*len(self.vertex_data)

	def write(self, file):
		self.format.write(file)
		# write array length
		file.write(struct.pack("1I", len(self.vertex_data)))
		packed_vertex_array = b''
		for vertex in self.vertex_data:
			packed_vertex_array += struct.pack(self.format.getVertexFormatString(), *vertex)
		file.write(packed_vertex_array)

class MDLMesh:
	def __init__(self, vertex_array_index, triangle_batches):
		self.vertex_array_index = 0
		self.triangle_batches = triangle_batches

class MDLTriangleBatch:
	def __init__(self, mat_index):
		self.mat_index = mat_index
		self.indices = []



"""
model format:
scene graph
-- basically used for instancing + bounding volumes
vertex data // for every format
material data // consists of texture name
batches // index data
-- start of batch has texture info
animation data

each chunk has a magic num size
"""

# chunks

class MDLNodeChunk:
	def __init__(self, nodes):
		self.chunk_type = b"INF1"
		self.nodes = nodes

	def getSize(self):
		return struct.calcsize("4s2I") + len(self.nodes)*struct.calcsize("1I10f6f")

	def write(self, file):
		file.write(struct.pack("4s2I", self.chunk_type, self.getSize(), len(self.nodes)))
		for node in self.nodes:
			packed_node = struct.pack("1I", node.mesh_index)
			packed_node += struct.pack("3f", node.transform.location.x, node.transform.location.y, node.transform.location.z)
			packed_node += struct.pack("4f", node.transform.rotation[1], node.transform.rotation[2], node.transform.rotation[3], node.transform.rotation[0])
			packed_node += struct.pack("3f", node.transform.scale.x, node.transform.scale.y, node.transform.scale.z)
			packed_node += struct.pack("3f", node.aabb.min.x, node.aabb.min.y, node.aabb.min.z)
			packed_node += struct.pack("3f", node.aabb.max.x, node.aabb.max.y, node.aabb.max.z)
			file.write(packed_node)

class MDLMaterialChunk:
	def __init__(self, materials):
		self.chunk_type = b"MAT1"
		self.materials = materials

	def getSize(self):
		return struct.calcsize("4s2I") + 64*len(self.materials)

	def write(self, file):
		file.write(struct.pack("4s2I", self.chunk_type, self.getSize(), len(self.materials)))
		for mat in self.materials:
			file.write(struct.pack("63sx", mat.filepath.encode("utf-8")))

class MDLSkeletonChunk:
	def __init__(self, bones):
		self.chunk_type = b"SKL1"
		self.bones = bones # TODO: multiple skeletons? right now only the first

	def getSize(self):
		return struct.calcsize("4s2I") + MDLBone.getSize() * len(self.bones)

	def write(self, file):
		file.write(struct.pack("4s2I", self.chunk_type, self.getSize(), len(self.bones)))
		for bone in self.bones:
			bone.write(file)

class MDLActionChunk:
	def __init__(self, actions):
		self.chunk_type = b"ACT1"
		self.actions = actions

	def getSize(self):
		size = struct.calcsize("4s2I")
		for action in self.actions:
			size += action.getSize()
		return size

	def write(self, file):
		file.write(struct.pack("4s2I", self.chunk_type, self.getSize(), len(self.actions)))
		for action in self.actions:
			action.write(file)

class MDLVertexChunk:
	def __init__(self, vertex_arrays):
		self.chunk_type = b"VTX1"
		self.vertex_arrays = vertex_arrays

	def getSize(self):
		size = struct.calcsize("4s2I") # header
		for vertex_array in self.vertex_arrays:
			size += vertex_array.getSize() # vertex format + array length
		return size

	def write(self, file):
		file.write(struct.pack("4s2I", self.chunk_type, self.getSize(), len(self.vertex_arrays)))
		for vertex_array in self.vertex_arrays:
			vertex_array.write(file)

# TODO: implement other data types for indices
class MDLTriangleChunk:
	def __init__(self, meshes):
		self.chunk_type = b"TRI1"
		self.meshes = meshes

	def getSize(self):
		size = struct.calcsize("4s2I")
		# TODO: calc size
		for mesh in self.meshes:
			size += 12
			for batch in mesh.triangle_batches:
				size += 12
				size += 2*len(batch.indices)
		# padding
		if size % 4 != 0:
			size += 2
		return size;

	def write(self, file):
		file.write(struct.pack("4s2I", self.chunk_type, self.getSize(), len(self.meshes)))
		triangle_index = 0
		for mesh in self.meshes:
			file.write(struct.pack("3I", mesh.vertex_array_index, DataType['DT_UNSIGNED_SHORT'], len(mesh.triangle_batches)))
			for batch in mesh.triangle_batches:
				triangle_count = len(batch.indices)
				file.write(struct.pack("1i2I", batch.mat_index, triangle_index, triangle_count))
				triangle_index += triangle_count

		file.write(struct.pack("1I", triangle_index)) # index count
		packed_index_data = b''
		for mesh in self.meshes:
			for triangle_batch in mesh.triangle_batches:
				packed_index_data += struct.pack("%dH" % len(triangle_batch.indices), *triangle_batch.indices)
		# align data to 4 bytes
		if (triangle_index%2) != 0:
			packed_index_data += struct.pack("2x")
		file.write(packed_index_data)


# mdl model

class MDLModel:
	def __init__(self):
		self.chunks = []

	def addChunk(self, chunk):
		self.chunks.append(chunk)

	def export(self, filepath):
		file = open(filepath, "wb")

		magic_num = b"MDL1"
		version = 1

		chunk_count = len(self.chunks)

		header_format = "4s3I" # magic, version, file_size, chunk_count
		header_size = struct.calcsize(header_format)
		file_size = header_size
		for chunk in self.chunks:
			file_size += chunk.getSize()

		# calculate chunk offsets
		#chunk_offsets = 4*[0]
		#chunk_offsets[0] = header_size
		#for i, chunk in enumerate(chunks):
		#	if i+1 < chunk_count:
		#		chunk_offset[i+1] = chunk_offsets[i]+chunk.getSize()
		#file_size = chunk_offsets[-1] + chunks[-1].getSize();

		#write header
		file.write(struct.pack(header_format, magic_num, version, file_size, chunk_count))

		# write scene graph chunk
		for chunk in self.chunks:
			chunk.write(file)

		file.flush()
		file.close()
