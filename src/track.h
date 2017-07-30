// invariant: track is generated as such it never reverses in y direction

struct TrackSegment {
	vec2 p; // base point
	vec3 dims; // x: width, y: length, z: height

	vec2 dir; // normalized
	vec2 t; // tangent direved from dir

	float distance; // accumulated distance
};

const int TR_MAX_VERTEX_COUNT = 1024;

class Track {
public:
	void init();

	// difficulty 0: no obstacles, 1: full of obstacles
	void generate(float difficulty);
	float length; // in meters

	bool traceZ(vec2 p, float *z, float *distance = nullptr); // true if on track

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