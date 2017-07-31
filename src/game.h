class Game {
public:
	VideoMode video;
	Camera camera;

	Player player; // the car

	bool gameover;
	bool quit;

	Track tracks[2];
	int current_track_idx;

	int level;

	void init();
	void reset();
	void destroy();

	void updateCamera(float delta_time);

	void drawHUD();

	void tick(float delta_time);
};