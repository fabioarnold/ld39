class Game {
public:
	VideoMode video;

	bool quit;

	void init();
	void destroy();

	void tick(float delta_time);
};