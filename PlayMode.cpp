#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint maze_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > maze_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("maze.pnct"));
	maze_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > maze_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("maze.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = maze_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = maze_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

bool PlayMode::ball_collides_wall(glm::vec3 world_ball_pos) {

	if(!(-1.75 < world_ball_pos.x && world_ball_pos.x < 1.75 && -1.75 < world_ball_pos.y && world_ball_pos.y < 1.75)) {
		return true;
	}

	for( Scene::Transform &transform: scene.transforms) {
		
		if (transform.name.substr(0, 5) == "Wall.") {

			Mesh const &wall = maze_meshes->lookup(transform.name);
      
			glm::mat4x3 to_world = transform.make_local_to_world();
			glm::vec3 world_wall_min = glm::vec3(to_world * glm::vec4(wall.min, 1.0f));
			glm::vec3 world_wall_max = glm::vec3(to_world * glm::vec4(wall.max, 1.0f));
			if(world_wall_min.x > world_wall_max.x) {
				std::swap(world_wall_min.x, world_wall_max.x);
			}
			if(world_wall_min.y > world_wall_max.y) {
				std::swap(world_wall_min.y, world_wall_max.y);
			}

			float radius = 0.15f;
      glm::vec3 world_ball_min = world_ball_pos - glm::vec3(radius, radius, radius);
      glm::vec3 world_ball_max = world_ball_pos + glm::vec3(radius, radius, radius);

      if ((world_ball_min.x < world_wall_min.x && world_wall_max.x < world_ball_max.x && !(world_ball_min.y > world_wall_max.y || world_ball_max.y < world_wall_min.y) ) ||
          (world_ball_min.y < world_wall_min.y && world_wall_max.y < world_ball_max.y && !(world_ball_min.x > world_wall_max.x || world_ball_max.x < world_wall_min.x))) {
        return true;
      }
    }
  }    
	return false;
}

PlayMode::PlayMode() : scene(*maze_scene) {

	for (auto &drawable : scene.drawables) {
	 	if (drawable.transform->name == "Ball") ball = drawable.transform;
		else if(drawable.transform->name == "Present") present = drawable.transform;
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}else if (evt.key.keysym.sym == SDLK_a) {
			a.downs += 1;
			a.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			d.downs += 1;
			d.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			w.downs += 1;
			w.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			s.downs += 1;
			s.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			a.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			d.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			w.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			s.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	if(win){
		return;
	}
	
	//move ball:
	{

		//combine inputs into a move:
		constexpr float BallSpeed = 15.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * BallSpeed * elapsed;

		glm::mat4x3 frame = ball->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_forward = frame[1];

		glm::vec3 new_position = ball->position + move.x * frame_right + move.y * frame_forward;
		if(!ball_collides_wall(new_position)) {
			ball->position = new_position;
		}
	}

	{
		if(std::abs(ball->position.x + 1.58) < 0.1 && std::abs(ball->position.y-1.26) < 0.1) {
			win = true;
			present->scale = glm::vec3(0,0,0);
		}
	}

	{

		//combine inputs into a move:
		constexpr float CameraSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (a.pressed && !d.pressed) move.x =-1.0f;
		if (!a.pressed && d.pressed) move.x = 1.0f;
		if (s.pressed && !w.pressed) move.y =-1.0f;
		if (!s.pressed && w.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * CameraSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

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
	// TODO: consider using the Light(s) in the scene to do this
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

	GL_ERRORS(); //print any errors produced by this setup code

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
		lines.draw_text("WASD moves the camera; arrows move the snowman",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves the camera; arrows move the snowman",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
