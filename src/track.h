struct TrackChunk {
	// geometry
};

const int TR_MAX_VERTEX_COUNT = 1024;

class Track {
public:
	void init();

	// difficulty 0: no obstacles, 1: full of obstacles
	void generate(float difficulty);
	void draw(mat4 view_proj_mat);

private:
	float *_vertex_data = nullptr;
	int _vertex_count;

	Shader _shader;
	GLint _mvp_loc;
	GLint _color_loc;
	GLuint _vbo = 0;
};