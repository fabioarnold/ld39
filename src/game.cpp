struct sth_stash* font_stash = nullptr;
const int FONT_STASH_SIZE = 512;
int font_opensans = 0;

MDLModel car_model;

void Game::init() {
	// setup gl
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

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

	// load meshes
	car_model.load("data/models/car.mdl");

	// generate track
	track.init();
	track.generate(0.0f);
}

void Game::destroy() {
	// free gl resources
	car_model.destroy();
}

void Game::tick(float delta_time) {
	static int counter = 0;
	counter++;
	if (counter > 60) {
		track.generate(0.0f);
		counter = 0;
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	camera.field_of_view = 0.25f * (float)M_PI; // 45°
	camera.aspect_ratio = (float)video.width / (float)video.height;
	camera.setPerspectiveProjection(0.1f, 1000.0f);
	camera.location = v3(0.0f, -6.0f, 58.0f); //v3(0.0f, -50.0f, 20.0f);
	camera.euler_angles.x = -0.39f * (float)M_PI; // 90°
	camera.updateRotationMatrix();
	camera.updateViewMatrix();
	camera.updateViewProjectionMatrix();

	static float angle = 0.0f;
	mat4 car_mat = translationMatrix(v3(0.0f, 4.0f, 50.0f)) * m4(rotationMatrix(v3(0.0f, 0.0f, 1.0f), angle));
	angle += delta_time;

	track.draw(camera.view_proj_mat);
	car_model.draw(camera.view_proj_mat * car_mat);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	debug_renderer.render(camera.view_proj_mat);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

#if 0
	if (fmodf(angle, 0.2f) > 0.1f) {
		const char *text = "Super Death Race 9000";
		vec3 text_pos = v3((float)video.width/2, (float)video.height/4, 0.0f);
		const int font_size = 80;
		vec4 font_color = v4(1.0f);
		mat4 proj_mat = makeOrtho(0.0f, (float)video.width, 0.0f, (float)video.height, -1.0f, 1.0f);

		glDisable(GL_DEPTH_TEST);
		sth_begin_draw(font_stash, proj_mat.e);
		float minx, miny, maxx, maxy;
		sth_dim_text(font_stash, font_opensans, font_size, text, &minx, &miny, &maxx, &maxy);
		text_pos.x = floorf(text_pos.x - 0.5f * (maxx-minx));
		text_pos.y = floorf(text_pos.y - 0.5f * (maxy-miny));
		float dx = 0.0f;
		sth_draw_text(font_stash, font_opensans, font_size, video.pixel_scale, text_pos.e, font_color.e, text, &dx);
		sth_end_draw(font_stash);
		glEnable(GL_DEPTH_TEST);
	}
#endif
}