struct PlayerControls {
	ButtonState button_steer_left;
	ButtonState button_steer_right;
	ButtonState button_accelerate;
	ButtonState button_decelerate;
};

const float OIL_SPILL_DURATION = 0.5f;

class Track;

struct Player {
	PlayerControls controls;

	vec3 position;
	float speed;
	float z_angle;

	// state
	float oil_spill = 0.0f; // if >0 in effect -> no control

	static MDLModel car_model;
	MDLAction *idle_action;
	MDLAction *steer_left_action;
	MDLAction *steer_right_action;

	void init();

	void onOilSpill() {if (oil_spill == 0.0f) oil_spill = OIL_SPILL_DURATION;}

	bool centerOnTrack;
	bool leftOnTrack;
	bool rightOnTrack;
	float timeOffTrack = 0.0f;
	void checkTrack(Track *track);

	void tick(float delta_time);
	void draw(mat4 view_proj);
};