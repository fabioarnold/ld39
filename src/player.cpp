void Player::init() {
	idle_action = car_model.getActionByName("idle");
	steer_left_action = car_model.getActionByName("steer_left");
	steer_right_action = car_model.getActionByName("steer_right");

	z_angle = angleFromDir(v2(0.0f, 1.0f));
	position.y = 4.0;
	position.z = 50.0f;
}

void Player::checkTrack(Track *track) {
	float z;
	vec2 t = dirFromAngle(z_angle - 0.5f*(float)M_PI);
	leftOnTrack = track->traceZ(v2(position)-t, &z);
	rightOnTrack = track->traceZ(v2(position)+t, &z);
	centerOnTrack = track->traceZ(v2(position), &z);
	if (centerOnTrack) {
		position.z = z;
	} else {
		position.z = 0.0f;
	}
}

void Player::tick(float delta_time) {
	const float MAX_STEERING_ANGLE = 0.125f * (float)M_PI; // 22.5°

	bool acceleration_disabled = false;
	bool steering_disabled = false;

	// handle effects
	if (oil_spill > 0.0f) {
		oil_spill -= delta_time;
		if (oil_spill < 0.0f) oil_spill = 0.0f;
		// disable steering
		steering_disabled = true;
		acceleration_disabled = true;
	}

	// off track stuff
	if (!leftOnTrack || !rightOnTrack) timeOffTrack += delta_time;
	else timeOffTrack = 0.0f;

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
	float steering_angle = 0.0f;
	if (controls.button_steer_left.down() && !controls.button_steer_right.down()) steering_angle = MAX_STEERING_ANGLE;
	if (controls.button_steer_right.down() && !controls.button_steer_left.down()) steering_angle = -MAX_STEERING_ANGLE;

	// apply drag to speed
	speed *= 0.998f;

	// HACK: limit steering based on speed
	float max_speed = 24.0f / 0.5f;
	steering_angle *= (max_speed - speed) / max_speed;

	// figure out local wheel position (simplified as bicycle)
	vec2 front_wheel_pos = dirFromAngle(z_angle);
	vec2 back_wheel_pos = -0.9f * dirFromAngle(z_angle);

	position -= v3(front_wheel_pos + back_wheel_pos, 0.0f); // substract old positions

	// move wheels individually
	front_wheel_pos += delta_time * speed * dirFromAngle(z_angle + (steering_disabled ? 0.0f : steering_angle));
	back_wheel_pos += delta_time * speed * dirFromAngle(z_angle);

	position += v3(front_wheel_pos + back_wheel_pos, 0.0f); // add new positions

	// calc new heading
	z_angle = angleFromDir(normalize(front_wheel_pos - back_wheel_pos));


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
	float angle = z_angle - 0.5f * (float)M_PI;
	if (oil_spill > 0.0f) {
		angle += 2.0f*(float)M_PI * oil_spill/OIL_SPILL_DURATION;
	}
	mat4 car_mat = translationMatrix(position) * m4(rotationMatrix(v3(0.0f, 0.0f, 1.0f), angle));
	if (!leftOnTrack || !rightOnTrack) {
		float c = cosf(10.0f*timeOffTrack);
		if (!leftOnTrack) c = c - 1.0f;
		else c = 1.0f - c;
		angle = fminf(timeOffTrack, 0.25f)*c;
		car_mat = car_mat * m4(rotationMatrix(v3(0.0f, 1.0f, 0.0f), angle));
	}
	car_model.draw(view_proj_mat * car_mat);
}