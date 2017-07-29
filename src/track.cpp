const int TR_VA_POSITION = 0;
const int TR_VA_NORMAL = 1;

void Track::init() {
	char vert_source[] = {
		"uniform mat4 mvp;							\n"
		"attribute vec3 position;					\n"
		"attribute vec3 normal;						\n"
		"varying vec3 v_normal;						\n"
		"varying float v_abyss;						\n"
		"void main() {								\n"
		"	v_normal = normal;						\n"
		"	v_abyss = 1.0;							\n"
		"	if (position.z < 1.0) v_abyss = 0.0;	\n"
		"	gl_Position = mvp * vec4(position, 1.0);\n"
		"}											\n"
	};

	char frag_source[] = {
		"#ifdef GL_ES								\n"
		"precision mediump float;					\n"
		"#endif										\n"
		"uniform vec4 color;						\n"
		"varying vec3 v_normal;						\n"
		"varying float v_abyss;						\n"
		"void main() {								\n"
		"	vec3 light = normalize(vec3(0.2, 0.3, -1.0));\n"
		"	vec3 normal = normalize(v_normal);		\n"
		"	float shade = 0.75 + 0.25*dot(normal, -light);\n"
		"	float dist = (gl_FragCoord.z / gl_FragCoord.w) / 400.0;\n"
		"	float fog = 1.0f - dist*dist;\n"
		"	shade *= v_abyss * fog;\n"
		"	gl_FragColor = vec4(shade*color.rgb, color.a);\n"
		"}											\n"
	};
	_shader.compileAndAttach(GL_VERTEX_SHADER, vert_source);
	_shader.compileAndAttach(GL_FRAGMENT_SHADER, frag_source);
	_shader.bindVertexAttrib("position", TR_VA_POSITION);
	_shader.bindVertexAttrib("normal", TR_VA_NORMAL);
	_shader.link();
	_shader.use();
	_mvp_loc = _shader.getUniformLocation("mvp");
	_color_loc = _shader.getUniformLocation("color");
}

float rand_rangef(float min, float max) {
	return min + randf() * (max - min);
}


struct Segment {
	vec2 p; // base point
	vec3 dims; // width, length, height

	vec2 dir; // normalized
	vec2 t; // tangent direved from dir
};

void Track::generate(float difficulty) {
	const float max_distance = 1000.0f; // 1km
	const float segment_min_width = 6.0f;
	const float segment_max_width = 12.0f;
	const float segment_min_length = 10.0f;
	const float segment_max_length = 30.0f;
	const float segment_min_height = 10.0f;
	const float segment_max_height_delta = 2.0f;
	const float segment_angle_max_delta = 0.125f * (float)M_PI; // 22.5°

	float distance = 0.0f; // current distance

	// generate a path of segments
	std::vector<Segment> segments;

	Segment s; // current segment
	s.p = v2(0.0f);
	s.dir = v2(0.0f, 1.0f);
	s.t = v2(s.dir.y, -s.dir.x);
	s.dims.y = segment_max_length; // initial length
	s.dims.z = 50.0f; // initial height

	while (distance < max_distance) {
		s.dims.x = rand_rangef(segment_min_width, segment_max_width);
		distance += s.dims.y;

		segments.push_back(s);

		s.p = s.p + s.dims.y * s.dir;

		s.dims.y = rand_rangef(segment_min_length, segment_max_length);
		s.dims.y = fminf(s.dims.y, max_distance - distance + 0.1f); // clamp
		s.dims.z = s.dims.z + randf() * segment_max_height_delta;

		float angle = angleFromDir(s.dir);
		angle += rand_rangef(-segment_angle_max_delta, segment_angle_max_delta); // modify angle

		s.dir = dirFromAngle(angle);
		s.dir = mix(s.dir, v2(0.0f, 1.0f), 0.1f * (1.0f - s.dir.y*s.dir.y)); // straighten
		if (s.dir.y < 0.0f) s.dir.y = 0.0f; // clamp to move in pos y dir

		s.dir = normalize(s.dir);
		s.t = v2(s.dir.y, -s.dir.x);
	}

	// generate mesh from path
	std::vector<vec3> points;
	std::vector<int> indices;
	const int POINTS_PER_SEGMENT = 4;
	points.reserve(segments.size()*POINTS_PER_SEGMENT);
	for (size_t i = 0; i < segments.size(); i++) {
		s = segments[i];
		points.push_back(v3(s.p - 0.5f * s.dims.x * s.t, 0.0f));
		points.push_back(v3(s.p - 0.5f * s.dims.x * s.t, s.dims.z));
		points.push_back(v3(s.p + 0.5f * s.dims.x * s.t, s.dims.z));
		points.push_back(v3(s.p + 0.5f * s.dims.x * s.t, 0.0f));
	}
	indices.reserve((segments.size() - 1)*3*6);
	for (int i = 0; i < (int)segments.size() - 1; i++) {
		for (int j = 0; j < 3; j++) {
			indices.push_back(POINTS_PER_SEGMENT * i + j);
			indices.push_back(POINTS_PER_SEGMENT * i + j + 1);
			indices.push_back(POINTS_PER_SEGMENT * (i+1) + j);

			indices.push_back(POINTS_PER_SEGMENT * (i+1) + j + 1);
			indices.push_back(POINTS_PER_SEGMENT * (i+1) + j);
			indices.push_back(POINTS_PER_SEGMENT * i + j + 1);
		}
	}

	#if 0 // indexed debug drawing
	debug_renderer.setColor(0.0f, 0.0f, 0.0f, 1.0f);
	for (size_t i = 0; i < indices.size(); i += 3) {
		int i0 = indices[i];
		int i1 = indices[i+1];
		int i2 = indices[i+2];
		debug_renderer.drawTriangle(points[i0], points[i1], points[i2]);
	}
	#endif

	// make mesh with per face normals
	_vertex_count = (int)indices.size();
	int face_count = _vertex_count / 3;
	ARRAY_FREE(_vertex_data);
	_vertex_data = new float[6*_vertex_count]; // for position and normal
	vec3 p[3]; float *vdp = _vertex_data; // pointer to current vertex
	for (int fi = 0; fi < face_count; fi++) {
		p[0] = points[indices[3*fi+0]];
		p[1] = points[indices[3*fi+1]];
		p[2] = points[indices[3*fi+2]];
		vec3 nor = normalize(cross(p[1]-p[0], p[2]-p[0]));

		for (int vi = 0; vi < 3; vi++) {
			vdp[0] = p[vi].x; vdp[1] = p[vi].y; vdp[2] = p[vi].z;
			vdp[3] = nor.x;   vdp[4] = nor.y;   vdp[5] = nor.z;
			vdp += 6;
		}
	}

	// upload vertex data
	if (!_vbo) glGenBuffers(1, &_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * indices.size(), _vertex_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Track::draw(mat4 view_proj_mat) {
	//if (_vertex_count == 0) return;

	//glDisable(GL_DEPTH_TEST);

	ImGui::Begin("track");
	static float _difficulty = 0.0f;
	ImGui::SliderFloat("difficulty", &_difficulty, 0.0f, 1.0f);
	if (ImGui::Button("generate")) generate(_difficulty);
	ImGui::End();

	if (!_vbo) return;

	glBindBuffer(GL_ARRAY_BUFFER, _vbo);

	// configure vertex array
	glEnableVertexAttribArray((GLuint)TR_VA_POSITION);
	glVertexAttribPointer((GLuint)TR_VA_POSITION, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (GLvoid*)0);
	glEnableVertexAttribArray((GLuint)TR_VA_NORMAL);
	glVertexAttribPointer((GLuint)TR_VA_NORMAL, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (GLvoid*)(3*sizeof(float)));

	// configure shader
	_shader.use();
	glUniformMatrix4fv(_mvp_loc, 1, GL_FALSE, view_proj_mat.e);
	glUniform4f(_color_loc, 0.9f, 0.85f, 0.6f, 1.0f);

	glDrawArrays(GL_TRIANGLES, 0, _vertex_count);

	glDisableVertexAttribArray((GLuint)TR_VA_POSITION);
	glDisableVertexAttribArray((GLuint)TR_VA_NORMAL);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}