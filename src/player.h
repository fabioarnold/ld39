struct PlayerControls {
	ButtonState button_steer_left;
	ButtonState button_steer_right;
	ButtonState button_accelerate;
	ButtonState button_decelerate;
};

struct Player {
	PlayerControls controls;

	vec3 position;
	float speed;
	float z_angle;

	static MDLModel car_model;
	MDLAction *idle_action;
	MDLAction *steer_left_action;
	MDLAction *steer_right_action;

	void init();

	void tick(float delta_time);
	void draw(mat4 view_proj);
};