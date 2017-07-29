class Game {
public:
	VideoMode video;
	Camera camera;

	Player player; // the car

	bool quit;

	Track track;

	void init();
	void destroy();

	void tick(float delta_time);
};