MDLModel Player::car_model;
MDLModel Player::explosion_model;

void Player::init() {
	idle_action = car_model.getActionByName("idle");
	steer_left_action = car_model.getActionByName("steer_left");
	steer_right_action = car_model.getActionByName("steer_right");

	// replace explosion shader
	{
		const char *vert_source = 
		"uniform mat4 mvp;"
		"uniform mat4 bone_mats[16];"
		"attribute vec3 va_position;"
		"attribute vec2 va_texcoord0;"
		"attribute vec4 va_bone_indices;"
		"attribute vec3 va_bone_weights;"
		"varying vec3 v_normal;"
		"varying vec2 v_texcoord0;"
		"void main() {"
		"\tv_texcoord0 = va_texcoord0;"
		"\tivec4 bone_indices = ivec4(va_bone_indices);"
		"\tfloat bone_weight3 = 1.0 - va_bone_weights[0] - va_bone_weights[1] - va_bone_weights[2];"
		"\tmat4 bone_mat = va_bone_weights[0] * bone_mats[bone_indices[0]];"
		"\tbone_mat += va_bone_weights[1] * bone_mats[bone_indices[1]];"
		"\tbone_mat += va_bone_weights[2] * bone_mats[bone_indices[2]];"
		"\tbone_mat += bone_weight3 * bone_mats[bone_indices[3]];"
		"\tgl_Position = mvp*bone_mat*vec4(va_position, 1.0);"
		"}";

		const char *frag_source =
		"#ifdef GL_ES\n"
		"precision mediump float;\n"
		"#endif\n"
		"uniform sampler2D colormap;"
		"varying vec2 v_texcoord0;"
		"void main() {"
		"\tvec4 color = texture2D(colormap, v_texcoord0);"
		"if (color.a < 0.01) discard;"
		"\tgl_FragColor = vec4(color.rgb, color.a);"
		"}";

		explosion_model.shader.destroy();
		explosion_model.shader.compileAndAttach(GL_VERTEX_SHADER, vert_source);
		explosion_model.shader.compileAndAttach(GL_FRAGMENT_SHADER, frag_source);
		explosion_model.vertex_arrays[0].format.bindShaderAttribs(&explosion_model.shader);
		explosion_model.shader.link();
		explosion_model.shader.use();
		explosion_model.mvp_loc = explosion_model.shader.getUniformLocation("mvp");
		explosion_model.normal_mat_loc = explosion_model.shader.getUniformLocation("normal_mat");
		explosion_model.bone_mats_loc = explosion_model.shader.getUniformLocation("bone_mats");
		explosion_model.colormap_loc = explosion_model.shader.getUniformLocation("colormap");
		glBindTexture(GL_TEXTURE_2D, explosion_model.textures[0]);
		setFilterTexture2D(GL_NEAREST, GL_NEAREST);
	}
	spin_action = explosion_model.getActionByName("spin");

	reset();
}

void Player::reset() {
	alive = true;
	fell_off_track = false;
	exploded = false;
	oil_spill = 0.0f;

	fuel = 1.0f;
	distance = 0.0f;

	heading = angleFromDir(v2(0.0f, 1.0f));
	position.y = 4.0;
	position.z = 50.0f;
}

void Player::checkTrack(Track *track) {
	if (!alive) return; // no need to do collision checks

	float z;
	vec2 t = dirFromAngle(heading - 0.5f*(float)M_PI);
	leftOnTrack = track->traceZ(v2(position)-t, &z);
	rightOnTrack = track->traceZ(v2(position)+t, &z);
	centerOnTrack = track->traceZ(v2(position), &z, &distance);
	if (centerOnTrack) {
		position.z = z;
	} else {
		onFellOffTrack();
	}
}

void Player::onFellOffTrack() {
	fell_off_track = true;
	alive = false;

	off_track_time = 0.0f;
	off_track_y_angle = 0.0f;

	// throw car off track
	float jump_up = 4.0f;
	float nudge = 5.0f;
	velocity = v3(speed * dirFromAngle(heading), jump_up);
	if (!leftOnTrack) velocity.x -= nudge;
	if (!rightOnTrack) velocity.x += nudge;
}

void Player::onExploded() {
	exploded = true;
	alive = false;

	explosion_center = position + v3(0.0f, 0.0f, 1.0f);
	explosion_time = 0.0f;
}

void Player::tick(float delta_time) {
	const float MAX_STEERING_ANGLE = 0.125f * (float)M_PI; // 22.5°

	bool acceleration_disabled = false;
	bool steering_disabled = false;

	// handle effects
	spin_action->frame++;
	spin_action->frame %= spin_action->frame_count-1;
	explosion_model.applyAction(spin_action);

	if (!alive) {
		if (fell_off_track) {
			off_track_time += delta_time;
			if (!exploded && off_track_time > 1.5f) onExploded();

			off_track_y_angle += delta_time * 2.0f * dot(dirFromAngle(heading - 0.5f*(float)M_PI), v2(velocity));
			velocity += delta_time * v3(0.0f, 0.0f, -10.0f); // gravity (shitty physics)
			position += delta_time * velocity;
		}
		if (exploded) {
			explosion_time += delta_time;
		}
		return;
	}
	if (oil_spill > 0.0f) {
		oil_spill -= delta_time;
		if (oil_spill < 0.0f) oil_spill = 0.0f;
		// disable steering
		steering_disabled = true;
		acceleration_disabled = true;
	}

	// off track stuff
	if (!leftOnTrack || !rightOnTrack) {
		timeHalfOffTrack += delta_time;
		if (timeHalfOffTrack > HALF_OFF_TRACK_DURATION) {
			onFellOffTrack();
		}
	} else {
		timeHalfOffTrack = 0.0f;
	}

	float acceleration = fmaxf(24.0f - 0.5f*speed, 0.0f); // actually acceleration is a curve...
	float deceleration = speed > 0.0f ? 60.0f : fmaxf(12.0f + 1.6f*speed, 0.0f); // braking or reverse gear

	if (acceleration_disabled) {
		acceleration = 0.0f;
		deceleration = 0.0f;
	}

	// controls
	if (controls.button_accelerate.down()) speed += delta_time * acceleration;
	if (controls.button_decelerate.down()) {
		float prev_speed = speed;
		speed -= delta_time * deceleration;
		if (prev_speed >= 0.0f && speed < 0.0f && !controls.button_decelerate.pressed()) {
			speed = 0.0f; // brake to zero, button needs to be pressed again to reverse
		}
	}
	if (!controls.button_accelerate.down() && !controls.button_decelerate.down()) {
		// apply engine brake
		float engine_brake_deceleration = 20.0f;
		if (speed > 0.0f) {
			speed -= delta_time * engine_brake_deceleration;
			if (speed < 0.0f) speed = 0.0f;
		}
		if (speed < -0.001f) {
			speed += delta_time * engine_brake_deceleration;
			if (speed > -0.001f) speed = -0.001f;
		}
	}

	float steering_angle = 0.0f;
	if (controls.button_steer_left.down() && !controls.button_steer_right.down()) steering_angle = MAX_STEERING_ANGLE;
	if (controls.button_steer_right.down() && !controls.button_steer_left.down()) steering_angle = -MAX_STEERING_ANGLE;

	// apply drag to speed
	speed *= 0.998f;

	// HACK: limit steering based on speed
	float max_speed = 24.0f / 0.5f;
	steering_angle *= (max_speed - speed) / max_speed;

	// figure out local wheel position (simplified as bicycle)
	vec2 front_wheel_pos = dirFromAngle(heading);
	vec2 back_wheel_pos = -0.9f * dirFromAngle(heading);

	position -= v3(front_wheel_pos + back_wheel_pos, 0.0f); // substract old positions

	// move wheels individually
	front_wheel_pos += delta_time * speed * dirFromAngle(heading + (steering_disabled ? 0.0f : steering_angle));
	back_wheel_pos += delta_time * speed * dirFromAngle(heading);

	position += v3(front_wheel_pos + back_wheel_pos, 0.0f); // add new positions

	// calc new heading
	heading = angleFromDir(normalize(front_wheel_pos - back_wheel_pos));


#ifdef DEBUG
	ImGui::Begin("car");
	ImGui::Text("speed: %f m/s", (double)speed);
	ImGui::Text("speed: %f km/h", (double)speed * 3.6);
	ImGui::End();
#endif

	// update animation
	car_model.applyAction(idle_action);
	if (steering_angle < 0.0f) car_model.blendAction(steer_right_action, -steering_angle / MAX_STEERING_ANGLE);
	else if (steering_angle > 0.0f) car_model.blendAction(steer_left_action, steering_angle / MAX_STEERING_ANGLE);
}

void Player::draw(mat4 view_proj_mat) {
	float z_angle = heading - 0.5f * (float)M_PI;
	float y_angle = 0.0f;

	if (oil_spill > 0.0f) {
		z_angle += 2.0f*(float)M_PI * oil_spill/OIL_SPILL_DURATION;
	}

	// on the brink off falling off track animation
	if (!leftOnTrack || !rightOnTrack) {
		float c = cosf(10.0f*timeHalfOffTrack);
		if (!leftOnTrack) c = c - 1.0f;
		else c = 1.0f - c;
		y_angle = fminf(timeHalfOffTrack, 0.25f)*c;
	}

	mat4 fell_off_track_mat = m4(1.0f);
	if (fell_off_track) { // spin car around y axis
		fell_off_track_mat = translationMatrix(v3(0.0f, 0.0f, 1.0f))
			*  m4(rotationMatrix(v3(0.0f, 1.0f, 0.0f), off_track_y_angle))
			* translationMatrix(v3(0.0f, 0.0f, -1.0f));
	}

	mat4 car_mat = translationMatrix(position)
		* m4(rotationMatrix(v3(0.0f, 0.0f, 1.0f), z_angle))
		* m4(rotationMatrix(v3(0.0f, 1.0f, 0.0f), y_angle))
		* fell_off_track_mat;
	if (!exploded) {
		car_model.draw(view_proj_mat * car_mat);
	}

	if (exploded && explosion_time < 1.0f) {
		float s = 2.0f + 4.0f*explosion_time;
		for (int i = 0; i < 5; i++) {
			vec3 p = 2.0f * v3(randf(), randf(), randf()) - v3(1.0f);
			float rot = (float)M_PI * randf();
			explosion_model.draw(view_proj_mat * translationMatrix(explosion_center + p + v3(0.0f, 0.0f, 1.0f)) * 
				m4(rotationMatrix(v3(0.0f, 0.0f, 1.0f), rot) * scaleMatrix(v3(s))));
		}
	}
}