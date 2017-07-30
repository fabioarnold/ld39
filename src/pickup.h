enum PickupType {
	PT_GAS_TANK,
	PT_OIL_SPILL
};

class Pickup {
public:
	PickupType type;
	bool active = true;
	vec3 position;

	float anim_time = 0.0f;

	static MDLModel gas_tank_model;
	static MDLModel oil_spill_model;

	Pickup(PickupType t, vec3 pos) : type(t), position(pos) {}

	void tryCollect(Player *p);

	void draw(mat4 view_proj_mat);
};