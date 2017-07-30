struct PlayerControls {
	ButtonState button_steer_left;
	ButtonState button_steer_right;
	ButtonState button_accelerate;
	ButtonState button_decelerate;
};

enum PlayerState {
	PS_ALIVE,
	PS_FELL_OFF_TRACK,
	PS_EXPLODED
};

const float OIL_SPILL_DURATION = 0.5f;
const float HALF_OFF_TRACK_DURATION = 1.0f;

class Track;

struct Player {
	PlayerControls controls;

	PlayerState state;

	vec3 position;
	float speed;
	float heading;

	// state
	float oil_spill = 0.0f; // if >0 in effect -> no control

	static MDLModel car_model;
	MDLAction *idle_action;
	MDLAction *steer_left_action;
	MDLAction *steer_right_action;

	void init();
	void reset();

	void onFellOffTrack();
	void onOilSpill() {if (fequal(oil_spill, 0.0f)) oil_spill = OIL_SPILL_DURATION;}

	// off track state
	vec3 velocity;
	float off_track_y_angle; // rotate based on velocity.y (TODO: think of better name)
	// /off track state

	bool centerOnTrack;
	bool leftOnTrack;
	bool rightOnTrack;
	float timeOffTrack = 0.0f; // if this reaches HALF_OFF_TRACK_DURATION -> fall off track
	void checkTrack(Track *track); // updates the *OnTrack flags

	void tick(float delta_time);
	void draw(mat4 view_proj);
};