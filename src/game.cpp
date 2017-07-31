struct sth_stash* font_stash = nullptr;
const int FONT_STASH_SIZE = 512;
int font_opensans = 0;

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
	Player::car_model.load("data/models/car.mdl");
	Player::explosion_model.load("data/models/explosion.mdl");
	Pickup::gas_tank_model.load("data/models/gas_tank.mdl");
	Pickup::oil_spill_model.load("data/models/oil_spill.mdl");

	Track::init();

	player.init();

	reset();
}

void Game::reset() {
	gameover = false;

	player.reset();
	camera.location = v3(0.0f);
	camera.euler_angles = v3(0.0f);

	level = 1;

	current_track_idx = 0;

	// generate tracks
	tracks[current_track_idx].generate(0.1f*(float)level);
	TrackSegment &s = tracks[current_track_idx].segments.back();
	tracks[1-current_track_idx].generate(0.1f*(float)(level+1), s.p+s.dir*s.dims.y, s.dir, s.dims.x);

	// place player on new track
	TrackSegment &s0 = tracks[current_track_idx].segments.front();
	player.position = v3(s0.p + 4.0f*s0.dir, s0.dims.z);
	player.heading = angleFromDir(s0.dir);
	player.speed = 0.0f;
	player.fuel = 1.0f;
}

void Game::destroy() {
	// free static gl resources
	player.car_model.destroy();
	player.explosion_model.destroy();
	Pickup::gas_tank_model.destroy();
	Pickup::oil_spill_model.destroy();
	Track::destroy();
}

static float camera_laziness = 0.2f;
void Game::updateCamera(float delta_time) {
	vec3 camera_target_location = player.position + v3(-11.0f * dirFromAngle(player.heading), 8.0f);
	float camera_target_y_angle = -player.heading + 0.5f * (float)M_PI;
	if (player.speed < 0.0f) {
		camera_target_y_angle -= (float)M_PI; // reversing
		camera_target_location = player.position + v3(11.0f * dirFromAngle(player.heading), 8.0f);
	}

	if (!player.fell_off_track) { // don't follow the player off track
		camera.location = mix(camera.location, camera_target_location, camera_laziness);
	}
	//float delta = wrapMPi(camera_target_y_angle - camera.euler_angles.y);
	//if (delta > 0.0f) delta = fminf(delta, 10.0f*delta_time);
	//if (delta < 0.0f) delta = fmaxf(delta, -10.0f*delta_time);
	//camera.euler_angles.y = wrapMPi(camera.euler_angles.y + delta);
	camera.euler_angles.y = mixAngles(camera.euler_angles.y, camera_target_y_angle, camera_laziness);

	camera.field_of_view = 0.25f * (float)M_PI; // 45°
	camera.aspect_ratio = (float)video.width / (float)video.height;
	camera.setPerspectiveProjection(0.1f, 1000.0f);
	camera.euler_angles.x = -0.39f * (float)M_PI; // 90°
	camera.updateRotationMatrix();
	camera.updateViewMatrix();
	camera.updateViewProjectionMatrix();
}

void Game::tick(float delta_time) {

#ifdef DEBUG
	ImGui::Begin("camera");
	ImGui::SliderFloat("laziness", &camera_laziness, 0.0f, 1.0f);
	ImGui::End();

	ImGui::Begin("track");
	static float track_difficulty = 0.5f;
	ImGui::SliderFloat("difficulty", &track_difficulty, 0.0f, 1.0f);
	if (ImGui::Button("generate")) tracks[current_track_idx].generate(track_difficulty);
	ImGui::End();

	ImGui::Begin("effects");
	if (ImGui::Button("explode")) player.onExploded();
	if (ImGui::Button("oilspill")) player.onOilSpill();
	ImGui::Checkbox("center", &player.centerOnTrack);
	ImGui::Checkbox("left", &player.leftOnTrack);
	ImGui::Checkbox("right", &player.rightOnTrack);
	ImGui::End();
#endif

	if (gameover) {
		// press any key
		if (player.controls.button_steer_left.clicked()  ||
			player.controls.button_steer_right.clicked() ||
			player.controls.button_accelerate.clicked()  ||
			player.controls.button_decelerate.clicked())
		{
			reset(); // restart game
		}
	} else {
		player.tick(delta_time);
		if (fequal(player.fuel, 0.0f)) {
			player.onExploded();
			gameover = true;
		}

		if (!player.alive && player.exploded && player.explosion_time > EXPLOSION_DURATION) {
			// spawn player back on track
			TrackSegment *s = tracks[current_track_idx].findNearestSegment(player.last_position_on_track);
			float d = dot(s->dir, player.last_position_on_track);
			d = fmaxf(1.0f, fminf(s->dims.y - 1.0f, d));
			player.respawn(v3(s->p + d*s->dir, s->dims.z), s->dir);
		}

		if (player.alive) {
			// collect pickups
			for (Pickup &p : tracks[current_track_idx].pickups) {
				p.tryCollect(&player);
			}
		}

		player.checkTrack(&tracks[current_track_idx]);
		// goal detection
		if (tracks[current_track_idx].findNearestSegment(player.last_position_on_track) == &tracks[current_track_idx].segments.back()) {
			
			//player.alive = false;
			current_track_idx = 1-current_track_idx; // make next current
			// place player on new track
			TrackSegment &s0 = tracks[current_track_idx].segments.front();
			player.position = v3(s0.p + 4.0f*s0.dir, s0.dims.z);
			player.heading = angleFromDir(s0.dir);
			player.speed = 0.0f;
			player.fuel = 1.0f;

			level++;

			// generate new next track
			TrackSegment &s = tracks[current_track_idx].segments.back();
			tracks[1-current_track_idx].generate(0.1f*(float)(level+1), s.p+s.dir*s.dims.y, s.dir, s.dims.x);
		}

		updateCamera(delta_time);
	}



	// draw everything
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (int i = 0; i < (int)ARRAY_COUNT(tracks); i++) {
		tracks[i].draw(camera.view_proj_mat);
	}
	player.draw(camera.view_proj_mat);

	drawHUD();
}

void drawRect(vec2 p, vec2 s) {
	drawQuad(v3(p), v3(p + v2(s.x, 0.0f)), v3(p + s), v3(p + v2(0.0f, s.y)));
}

// position, size, thickness
void drawBorder(vec2 p, vec2 s, float t) {
	drawRect(p, v2(s.x - t, t));
	drawRect(p + v2(s.x - t, 0.0f), v2(t, s.y - t));
	drawRect(p + v2(t, s.y - t), v2(s.x - t, t));
	drawRect(p + v2(0.0, t), v2(t, s.y - t));
}

// distance meter and fuel level
void Game::drawHUD() {
	float distance_left = (tracks[current_track_idx].length - player.distance);
	char text_buffer[32];
	sprintf(text_buffer, "in %dm", (int)distance_left);

	mat4 proj_mat = makeOrtho(0.0f, (float)video.width, 0.0f, (float)video.height, -1.0f, 1.0f);
	float font_size = ceilf(0.1f * (float)video.height);
	float padding = ceilf(0.025f * (float)video.height);
	vec4 font_color = v4(1.0f);

	float minx, miny, maxx, maxy;
	sth_dim_text(font_stash, font_opensans, font_size, "FUEL:", &minx, &miny, &maxx, &maxy);
	float text_fuel_width = maxx - minx;
	sth_dim_text(font_stash, font_opensans, font_size, "GOAL:", &minx, &miny, &maxx, &maxy);
	float text_goal_width = maxx - minx;
	sth_dim_text(font_stash, font_opensans, font_size, "LEVEL:", &minx, &miny, &maxx, &maxy);
	float text_level_width = maxx - minx;
	float max_text_width = fmaxf(fmaxf(text_fuel_width, text_goal_width), text_level_width);
	float text_fuel_x = padding + max_text_width - text_fuel_width;
	float text_goal_x = padding + max_text_width - text_goal_width;
	float text_level_x = padding + max_text_width - text_level_width;

	sth_begin_draw(font_stash, proj_mat.e);
	sth_draw_text(font_stash, font_opensans, font_size, video.pixel_scale, v3(text_goal_x, (float)video.height - font_size, 0.0f).e, font_color.e, "GOAL:", nullptr);
	sth_draw_text(font_stash, font_opensans, font_size, video.pixel_scale, v3(text_fuel_x, (float)video.height - 2.0f*font_size, 0.0f).e, font_color.e, "FUEL:", nullptr);
	sth_draw_text(font_stash, font_opensans, 0.3f*font_size, video.pixel_scale, v3(2.0f*padding + max_text_width, (float)video.height - 1.25f*font_size, 0.0f).e, font_color.e, text_buffer, nullptr);
	sprintf(text_buffer, "LEVEL: %d", level);
	sth_draw_text(font_stash, font_opensans, font_size, video.pixel_scale, v3(text_level_x, (float)video.height - 3.0f*font_size, 0.0f).e, font_color.e, text_buffer, nullptr);
	sth_end_draw(font_stash);

	debug_renderer.setColor(1.0f, 1.0f, 1.0f, 1.0f);

	vec2 fuel_meter_p = v2(2.0f * padding + max_text_width, (float)video.height - 2.0f*font_size);
	vec2 fuel_meter_s = v2((float)video.width - 3.0f*padding - max_text_width, 0.55f*font_size);
	float thickness = 0.125f * padding;

	// draw fuel meter
	drawBorder(fuel_meter_p, fuel_meter_s, thickness);

	// draw goal meter
	vec2 goal_meter_p = fuel_meter_p + v2(0.0f, font_size);
	vec2 goal_meter_s = v2(fuel_meter_s.x, 2.0f * thickness);
	drawRect(goal_meter_p, goal_meter_s);

	float goal_x = (goal_meter_s.x-thickness) * fminf(1.0f, fmaxf(0.0f, distance_left / tracks[current_track_idx].length));
	drawRect(goal_meter_p + v2(goal_x, 0.0f), v2(thickness, fuel_meter_s.y));
	// draw little flag
	float x = goal_meter_p.x + goal_x + thickness;
	float y0 = goal_meter_p.y + 0.666f * fuel_meter_s.y;
	float y1 = goal_meter_p.y + fuel_meter_s.y;
	debug_renderer.drawTriangle(v3(x, y0, 0.0f),
		 v3(x + 0.25f * fuel_meter_s.y, 0.5f * (y0+y1), 0.0f), 
		 v3(x, y1, 0.0f));

	// draw fuel meter filling
	vec2 fuel_fill = fuel_meter_s - v2(4.0f * thickness);
	fuel_fill.x *= fminf(1.0f, fmaxf(0.0f, player.fuel));
	debug_renderer.setColor(1.0f, 0.0f, 0.0f, 0.5f);
	drawRect(fuel_meter_p + v2(2.0f * thickness), fuel_fill);

	if (gameover) {
		debug_renderer.setColor(0.0f, 0.0f, 0.0f, 0.5f);
		drawRect(v2(0.0f), v2((float)video.width, (float)video.height));
		sth_dim_text(font_stash, font_opensans, 2.0f*font_size, "GAME OVER", &minx, &miny, &maxx, &maxy);
		vec3 center = v3(0.5f * (float)video.width, 0.5f * (float)video.height, 0.0f);
		center.x -= 0.5f * (maxx - minx);
		center.y -= 0.5f * (maxy - miny);
		sth_begin_draw(font_stash, proj_mat.e);
		sth_draw_text(font_stash, font_opensans, 2.0f*font_size, video.pixel_scale, center.e, font_color.e, "GAME OVER", nullptr);
		sth_end_draw(font_stash);
	}

	// abuse the debug renderer
	glDisable(GL_DEPTH_TEST);
	debug_renderer.render(proj_mat);
	glEnable(GL_DEPTH_TEST);
}