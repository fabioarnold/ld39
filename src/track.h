// invariant: track is generated as such it never reverses in y direction

struct TrackSegment {
	vec2 p; // base point
	vec3 dims; // x: width, y: length, z: height

	vec2 dir; // normalized
	vec2 t; // tangent direved from dir

	// also takes next segment's point and direction (np, ndir)
	bool traceZ(vec2 p, float *z, vec2 np, vec2 ndir); // true if on segment
};

const int TR_MAX_VERTEX_COUNT = 1024;

class Track {
public:
	void init();

	// difficulty 0: no obstacles, 1: full of obstacles
	void generate(float difficulty);

	bool traceZ(vec2 p, float *z); // true if on track

	void draw(mat4 view_proj_mat);

private:
	std::vector<TrackSegment> segments;

	float *_vertex_data = nullptr;
	int _vertex_count;

	Shader _shader;
	GLint _mvp_loc;
	GLint _color_loc;
	GLuint _vbo = 0;
};