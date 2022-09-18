#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "read_write_chunk.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <random>

GLuint snake_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > snake_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("snake.pnct"));
	snake_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > snake_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("snake.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = snake_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = snake_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

PlayMode::PlayMode() : scene(*snake_scene) {
	// Load rhythm
	std::filebuf fb;
	std::cout << "Loading rhythm\n";
	{
		std::vector< RhythmBeats > rhythm_table = std::vector< RhythmBeats >();
		fb.open(data_path("rhythm.chunk"), std::ios::in);
		std::istream rhythm_is(&fb);
		read_chunk(rhythm_is, "rhy0", &rhythm_table);
		fb.close();

		rhythm = rhythm_table[0];
		beat_index = 0;
	}

	std::cout << "Loading drawables\n";
	// Get pointers to key drawables for convenience:
	for (auto &drawable : scene.drawables) {
		if (drawable.transform->name == "head") {
			head = &drawable;
			snake_head = head->transform;
		}
		else if (drawable.transform->name == "body") body = &drawable;
		else if (drawable.transform->name == "apple") apple = &drawable;
	}
	if (head == nullptr) throw std::runtime_error("Head not found.");
	if (body == nullptr) throw std::runtime_error("Body not found.");
	if (apple == nullptr) throw std::runtime_error("Apple not found.");

	// Setup snake transform
	snake_body = std::deque<SnakeBody*>();
	move_buffer = std::deque<DirectionPivot*>();

	SnakeBody *snake_head_move = new SnakeBody();
	snake_head_move->transform = snake_head;
	snake_head_move->transform->position.z = 0;
	snake_head_move->dir = DirRight;
	snake_head_move->next_pivot = nullptr;
	snake_body.push_back(snake_head_move); 

	// Get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	apples = std::list<Scene::Drawable*>();
	spawn_apple();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	// song_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

float PlayMode::rand_float(float lo, float hi) {
	// Referenced StackOverflow https://stackoverflow.com/questions/5289613/generate-random-float-between-two-floats
	float rand_val = ((float) rand()) / (float) RAND_MAX; // Generates random between 0 and 1
	float diff = hi - lo;
	return lo + diff * rand_val;
}

float PlayMode::dir_distance(Direction dir, glm::vec3 pos1, glm::vec3 pos2) {
	switch (dir) {
		case DirUp:
			return pos2.y - pos1.y;
		case DirDown:
			return pos1.y - pos2.y;
		case DirLeft:
			return pos1.x - pos2.x;
		case DirRight:
			return pos2.x - pos1.x;
		default: return 0;
	}
}

void PlayMode::spawn_apple() {
	float random_xpos = rand_float(min_pos_val, max_pos_val);
	float random_ypos = rand_float(min_pos_val, max_pos_val);

	Scene::Drawable new_apple = Scene::Drawable(*apple);
	Scene::Transform *transform = new Scene::Transform();

	transform->position = glm::vec3(random_xpos, random_ypos, 0);
	transform->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	transform->scale = apple->transform->scale;

	new_apple.transform = transform;
	scene.drawables.push_back(new_apple);
	apples.push_back(&scene.drawables.back());
}

bool PlayMode::check_collision(Scene::Transform *obj1, Scene::Transform *obj2, float bound) {
	if (std::max(obj1->position.x, obj2->position.x) - bound > std::min(obj1->position.x, obj2->position.x) + bound ||
		std::max(obj1->position.y, obj2->position.y) - bound > std::min(obj1->position.y, obj2->position.y) + bound) 
	{
		return false;
	}
	return true;
}

void PlayMode::check_snake_eat() {
	// Knowing that the snakes and apple sizes are all 1
	std::vector<Scene::Drawable*> to_delete = std::vector<Scene::Drawable*>();

	for (auto &apple_draw : apples) {
		Scene::Transform *apple_t = apple_draw->transform;

		if (!check_collision(snake_head, apple_t, 1)) {
			// No intersection
			continue;
		}

		std::cout << "Intersection!\n";
		// Intersection - remove apple
		to_delete.push_back(apple_draw);

		// Add to snake body
		Scene::Drawable new_body = Scene::Drawable(*body);
		Scene::Transform *transform = new Scene::Transform();

		glm::vec3 last_snake_pos = snake_body.back()->transform->position;
		switch (snake_body.back()->dir) {
			case DirUp:
				transform->position = glm::vec3(last_snake_pos.x, last_snake_pos.y - 2, 0);
				break;
			case DirDown:
				transform->position = glm::vec3(last_snake_pos.x, last_snake_pos.y + 2, 0);
				break;
			case DirRight:
				transform->position = glm::vec3(last_snake_pos.x - 2, last_snake_pos.y, 0);
				break;
			case DirLeft:
				transform->position = glm::vec3(last_snake_pos.x + 2, last_snake_pos.y, 0);
				break;
		}

		transform->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		transform->scale = snake_body.back()->transform->scale;

		new_body.transform = transform;
		scene.drawables.push_back(new_body);

		SnakeBody *snake_body_move = new SnakeBody();
		snake_body_move->transform = transform;
		snake_body_move->transform->position.z = 0;
		snake_body_move->dir = snake_body.back()->dir;
		snake_body_move->next_pivot = snake_body.back()->next_pivot;
		snake_body.push_back(snake_body_move); 
	}

	for (auto &apple_draw : to_delete) {
		scene.drawables.remove_if([apple_draw](auto &draw){ return draw.transform == apple_draw->transform; });
		apples.remove(apple_draw);
		spawn_apple(); //TODO: REMOVE
	}
}

void PlayMode::check_snake_collision() {
	// Knowing that the snakes and apple sizes are all 1
	for (auto &snake_part : snake_body) {
		Scene::Transform *snake_part_t = snake_part->transform;

		if (snake_part_t == snake_head) {
			// Don't check head against head
			continue;
		}

		if (!check_collision(snake_head, snake_part_t, 0.5f)) {
			// No intersection
			continue;
		}

		std::cout << "Dead!!\n";
	}
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	// hip->rotation = hip_base_rotation * glm::angleAxis(
	// 	glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 1.0f, 0.0f)
	// );
	// upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );
	// lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );

	//move sound to follow leg tip position:
	// song_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	// Move snake:
	{
		bool swap_dir = false;
		Direction new_dir;
		if (left.pressed && !right.pressed && !down.pressed && !up.pressed && snake_body.front()->dir != DirRight) {
			swap_dir = true;
			new_dir = DirLeft;
		}
		if (!left.pressed && right.pressed && !down.pressed && !up.pressed && snake_body.front()->dir != DirLeft) {
			swap_dir = true;
			new_dir = DirRight;
		}
		if (!left.pressed && !right.pressed && down.pressed && !up.pressed && snake_body.front()->dir != DirUp) {
			swap_dir = true;
			new_dir = DirDown;
		}
		if (!left.pressed && !right.pressed && !down.pressed && up.pressed && snake_body.front()->dir != DirDown) {
			swap_dir = true;
			new_dir = DirUp;
		}

		if (swap_dir) {
			DirectionPivot *pivot = new DirectionPivot();
			pivot->dir = new_dir;
			pivot->pos = snake_head->position;
			pivot->next = nullptr;
			
			if (move_buffer.size() > 0) {
				move_buffer.back()->next = pivot;
			}
			move_buffer.push_back(pivot);

			for (auto &snake_part: snake_body) {
				if (snake_part->next_pivot == nullptr) {
					snake_part->next_pivot = pivot;
				}
			}
		}

		for (auto &snake_part: snake_body) {
			// Process movement buffer
			if (snake_part->next_pivot != nullptr) {
				// Signed distance based on the direction of movement
				float dist = dir_distance(snake_part->dir, snake_part->transform->position, snake_part->next_pivot->pos);
				if (dist <= 0) {
					snake_part->dir = snake_part->next_pivot->dir;
					snake_part->transform->position = snake_part->next_pivot->pos;

					dist = -dist;
					switch (snake_part->dir) {
						case DirUp:
							snake_part->transform->position.y += dist;
							break;
						case DirDown:
							snake_part->transform->position.y -= dist;
							break;
						case DirRight:
							snake_part->transform->position.x += dist;
							break;
						case DirLeft:
							snake_part->transform->position.x -= dist;
							break;
					}
					
					if (snake_part == snake_body.back()) {
						// At the end; can remove this next pivot
						move_buffer.pop_front();
					}
					snake_part->next_pivot = snake_part->next_pivot->next;
				}
			}

        	switch (snake_part->dir) {
				case DirUp:
					snake_part->transform->position.y += snake_speed * elapsed;
					break;
				case DirDown:
					snake_part->transform->position.y -= snake_speed * elapsed;
					break;
				case DirRight:
					snake_part->transform->position.x += snake_speed * elapsed;
					break;
				case DirLeft:
					snake_part->transform->position.x -= snake_speed * elapsed;
					break;
			}
		}

		check_snake_eat();
		check_snake_collision();
	}

	// { //update listener to camera position:
	// 	glm::mat4x3 frame = camera->transform->make_local_to_parent();
	// 	glm::vec3 frame_right = frame[0];
	// 	glm::vec3 frame_at = frame[3];
	// 	Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	// }

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("WASD moves the snake",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves the snake",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

// glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	// return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
// }
