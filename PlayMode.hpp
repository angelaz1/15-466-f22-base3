#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include <glm/glm.hpp>

#define MAX_BEATS_IN_SONG 5000

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	bool gameOver = false;

	// Rhythm representation
	struct RhythmBeats {
		std::uint32_t bpm;
		bool beats[MAX_BEATS_IN_SONG];
		uint32_t beat_count;
	};

	// Rhythm for the current game
	RhythmBeats rhythm;
	uint32_t beat_index = 0;
	float song_timer = 0;

	// Looped song:
	std::shared_ptr< Sound::PlayingSample > song_loop = nullptr;

	// Model drawables
	Scene::Drawable *head = nullptr;
	Scene::Drawable *body = nullptr;
	Scene::Drawable *apple = nullptr;
	Scene::Drawable *stem = nullptr;
	Scene::Drawable *leaf = nullptr;

	Scene::Transform *snake_head = nullptr;

	// Snake game logic
	enum Direction {
		DirUp, DirDown, DirLeft, DirRight
	};

	struct DirectionPivot {
		glm::vec3 pos;
		Direction dir;
		DirectionPivot *next;
	};

	std::deque<DirectionPivot*> move_buffer;

	struct SnakeBody {
		Direction dir;
		Scene::Transform *transform;
		DirectionPivot *next_pivot;
	};

	std::deque<SnakeBody*> snake_body;

	float snake_speed = 10;

	struct Apple {
		float life_timer;
		Scene::Drawable *drawable;
		Scene::Drawable *stem_drawable;
		Scene::Drawable *leaf_drawable;
	};

	float apple_lifetime = 2.0f;
	const float apple_max_z = 0.0f;
	const float apple_min_z = -2.0f;
	std::list<Apple*> apples;
	
	const float min_pos_val = -19;
	const float max_pos_val = 19;

	// Hunger
	float hunger = 0.0f;
	float max_hunger = 10.0f;
	float hunger_growth_rate = 0.25f;
	float apple_hunger_restore = 1.0f;

	//camera:
	Scene::Camera *camera = nullptr;

	/// Functions

	// Generates a random float between lo and hi
	float rand_float(float lo, float hi);

	// Find the distance along the direction
	float dir_distance(Direction dir, glm::vec3 pos1, glm::vec3 pos2);

	// Spawn an apple at a random position on the map
	void spawn_apple();

	// Remove given list of apples
	void remove_apples(std::vector<Apple*> to_delete);

	// Checks if collision occurs between the two given transforms
	bool check_collision(Scene::Transform *obj1, Scene::Transform *obj2, float bound);

	// Checks if snake head eats the apple; if so, grows the snake body
	void check_snake_eat();

	// Checks if snake head hits itself; if so, lose game
	void check_snake_collision();
};
