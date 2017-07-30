MDLModel Pickup::oil_spill_model;
MDLModel Pickup::gas_tank_model;

void Pickup::tryCollect(Player *p) {
	if (!active) return;

	float distance = length(position - p->position);
	switch (type) {
		case PT_GAS_TANK:
			if (distance < 2.0f) {
				p->onGasTank();
				active = false;
			}
			break;
		case PT_OIL_SPILL:
			if (distance < 3.0f) {
				p->onOilSpill();
				active = false;
				anim_time = 0.0f;
			}
			break;
	}
}

void Pickup::draw(mat4 view_proj_mat) {
	anim_time += 1.0f/60.0f;

	float z_angle = 0.0f;
	float scale = 1.0f;
	vec3 anim_pos = position;
	if (type == PT_GAS_TANK) {
		if (!active) return;
		z_angle = 3.0f*anim_time;
		anim_pos += v3(0.0f, 0.0f, 0.5f + 0.25f*sinf(4.0f * anim_time));
	} else if (type == PT_OIL_SPILL) {
		if (!active) scale = 1.0f - 2.0f * anim_time;
		if (scale < 0.0f) return;
	}

	mat4 model_mat = translationMatrix(anim_pos) * m4(scaleMatrix(v3(scale)) * rotationMatrix(v3(0.0f, 0.0f, 1.0f), z_angle));
	mat4 mvp = view_proj_mat * model_mat;

	switch (type) {
		case PT_GAS_TANK:
			gas_tank_model.draw(mvp);
			break;
		case PT_OIL_SPILL:
			oil_spill_model.draw(mvp);
			break;
	}
}