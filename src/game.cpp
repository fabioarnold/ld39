struct sth_stash* font_stash = nullptr;
const int FONT_STASH_SIZE = 512;
int font_opensans = 0;

void Game::init() {
	// init fonts
	font_stash = sth_create(FONT_STASH_SIZE, FONT_STASH_SIZE);
	if (!font_stash) {
		LOGE("Could not create font stash.");
		exit(1);
	}
	font_opensans = sth_add_font(font_stash, "data/fonts/OpenSans/OpenSans-Regular.ttf");
	if (font_opensans == -1) {
		LOGE("Could not load font: OpenSans/OpenSans-Regular.ttf");
		exit(1);
	}
}

void Game::destroy() {

}

void Game::tick(float delta_time) {
	glClear(GL_COLOR_BUFFER_BIT);

	const char *text = "Ludum Dare 39";
	vec3 text_pos = v3((float)video.width/2, (float)video.height/2, 0.0f);
	const int font_size = 80;
	vec4 font_color = v4(1.0f);
	mat4 proj_mat = makeOrtho(0.0f, (float)video.width, 0.0f, (float)video.height, -1.0f, 1.0f);

	sth_begin_draw(font_stash, proj_mat.e);
	float minx, miny, maxx, maxy;
	sth_dim_text(font_stash, font_opensans, font_size, text, &minx, &miny, &maxx, &maxy);
	text_pos.x = floorf(text_pos.x - 0.5f * (maxx-minx));
	text_pos.y = floorf(text_pos.y - 0.5f * (maxy-miny));
	float dx = 0.0f;
	sth_draw_text(font_stash, font_opensans, font_size, video.pixel_scale, text_pos.e, font_color.e, text, &dx);
	sth_end_draw(font_stash);
}