void Player::init() {
	idle_action = car_model.getActionByName("idle");
	steer_left_action = car_model.getActionByName("steer_left");
	steer_right_action = car_model.getActionByName("steer_right");

	z_angle = angleFromDir(v2(0.0f, 1.0f));
	position.y = 4.0;
	position.z = 50.0f;
}

void Player::tick(float delta_time) {
	const float MAX_STEERING_ANGLE = 0.125f * (float)M_PI; // 22.5°

	float acceleration = fmaxf(24.0f - 0.4f*speed, 0.0f); // actually acceleration is a curve...
	float deceleration = speed > 0.0f ? 60.0f : fmaxf(12.0f + 1.6f*speed, 0.0f); // braking or reverse gear

	// TODO: add timer for reversing

	if (controls.button_accelerate.down()) speed += delta_time * acceleration;
	if (controls.button_decelerate.down()) speed -= delta_time * deceleration;

	float steering_angle = 0.0f;
	if (controls.button_steer_left.down() && !controls.button_steer_right.down()) steering_angle = MAX_STEERING_ANGLE;
	if (controls.button_steer_right.down() && !controls.button_steer_left.down()) steering_angle = -MAX_STEERING_ANGLE;

	// apply drag to speed
	speed *= 0.998f;

	// oversteering
	float max_speed = 24.0f / 0.4f;
	steering_angle *= (max_speed - speed) / max_speed;

	z_angle += 0.5f * delta_time * speed * steering_angle;

#ifdef DEBUG
	ImGui::Begin("car");
	ImGui::Text("speed: %f m/s", (double)speed);
	ImGui::Text("speed: %f km/h", (double)speed * 3.6);
	ImGui::End();
#endif

	position += delta_time * speed * v3(dirFromAngle(z_angle), 0.0f);

	// update animation
	car_model.applyAction(idle_action);
	if (steering_angle < 0.0f) car_model.blendAction(steer_right_action, -steering_angle / MAX_STEERING_ANGLE);
	else if (steering_angle > 0.0f) car_model.blendAction(steer_left_action, steering_angle / MAX_STEERING_ANGLE);
}

void Player::draw(mat4 view_proj_mat) {
	mat4 car_mat = translationMatrix(position) * m4(rotationMatrix(v3(0.0f, 0.0f, 1.0f), z_angle - 0.5f * (float)M_PI));
	car_model.draw(view_proj_mat * car_mat);
}