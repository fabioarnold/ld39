struct PlayerControls {
	ButtonState button_steer_left;
	ButtonState button_steer_right;
	ButtonState button_accelerate;
	ButtonState button_decelerate;
};

/*
enum PlayerState {
	PS_ALIVE,
	PS_FELL_OFF_TRACK,
	PS_EXPLODED
};
*/

const float OIL_SPILL_DURATION = 0.5f;
const float EXPLOSION_DURATION = 1.0f;
const float HALF_OFF_TRACK_DURATION = 1.0f;

class Track;

struct Player {
	PlayerControls controls;

	//PlayerState state;
	bool alive;
	bool fell_off_track;
	bool exploded;

	float fuel;
	float distance; // distance in meters traveled on track

	vec3 position;
	float speed;
	float heading;

	static MDLModel car_model;
	MDLAction *idle_action;
	MDLAction *steer_left_action;
	MDLAction *steer_right_action;
	static MDLModel explosion_model;
	MDLAction *spin_action;

	void init();
	void reset();
	void respawn(vec3 p, vec2 dir);

	void onExploded();
	void onFellOffTrack();
	void onGasTank();
	void onOilSpill();

	// oil spill state
	float oil_spill = 0.0f; // if >0 in effect -> no control
	// /oil spill state

	// off track state
	vec3 velocity;
	float off_track_time;
	float off_track_y_angle; // rotate based on velocity.y (TODO: think of better name)
	// /off track state

	// explosion state
	float explosion_time;
	vec3 explosion_center;
	// /explosion state

	bool centerOnTrack;
	bool leftOnTrack;
	bool rightOnTrack;
	vec2 last_position_on_track;
	float timeHalfOffTrack = 0.0f; // if this reaches HALF_OFF_TRACK_DURATION -> fall off track
	void checkTrack(Track *track); // updates the *OnTrack flags

	void tick(float delta_time);
	void draw(mat4 view_proj);
};