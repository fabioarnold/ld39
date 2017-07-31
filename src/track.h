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
	static void init();
	static void destroy();

	static MDLModel finish_line_model;

	// difficulty 0: no obstacles, 1: full of obstacles
	void generate(float difficulty, vec2 sp = v2(0.0f), vec2 sdir = v2(0.0f, 1.0f), float swidth = 18.0f);
	float length; // in meters

	TrackSegment *findNearestSegment(vec2 p);
	bool traceZ(vec2 p, float *z, float *distance = nullptr); // true if on track

	void draw(mat4 view_proj_mat);

	std::vector<Pickup> pickups;
	std::vector<TrackSegment> segments;

private:
	float *_vertex_data = nullptr;
	int _vertex_count;

	static Shader _shader;
	static GLint _mvp_loc;
	static GLint _color_loc;
	GLuint _vbo = 0;
};