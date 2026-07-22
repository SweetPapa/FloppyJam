#include "platform.h"
#include "render.h"
#include "collision.h"
#include "player.h"
#include "camera.h"
#include "levels.h"
#include "gameplay.h"
#include "save.h"
#include "audio.h"
#include "ui.h"

#include "raylib.h"
#include "raymath.h"
#include <math.h>

static Vector3 mote_local(Vector3 origin, Vector3 local, float yaw, Vector3 lean_axis, float lean)
{
    local=Vector3RotateByAxisAngle(local,(Vector3){0,1,0},yaw);
    local=Vector3RotateByAxisAngle(local,lean_axis,lean);
    return Vector3Add(origin,local);
}

static void draw_mote(PbRenderer *renderer, Vector3 p, const PbPlayer *player,
                      float elapsed, bool flow_aura)
{
    Color body = (Color){245, 128, 142, 255};
    Color petal = (Color){255, 198, 91, 255};
    float petal_open = player->state == PB_PLAYER_GLIDING ? 1.15f : .72f;
    float speed = Vector3Length(player->velocity);
    float squash=player->grounded?1.0f+fminf(speed/60,.12f):.92f;
    float stretch=player->state==PB_PLAYER_BURST?1.45f:1;
    float horizontal=sqrtf(player->velocity.x*player->velocity.x+player->velocity.z*player->velocity.z);
    float yaw=atan2f(-player->facing.x,-player->facing.z);
    Vector3 direction=horizontal>.1f?(Vector3){player->velocity.x/horizontal,0,player->velocity.z/horizontal}:player->facing;
    Vector3 lean_axis={direction.z,0,-direction.x};
    float lean=fminf(horizontal/8.5f,1)*.24f;
    int i;
    pb_draw_mesh(&renderer->meshes,PB_MESH_SPHERE,p,lean_axis,lean*57.2958f,
                 (Vector3){1.3f*stretch,1.15f/squash,1.3f},body);
    pb_draw_mesh(&renderer->meshes, PB_MESH_SPHERE,
                 mote_local(p,(Vector3){-.23f,.18f,-.55f},yaw,lean_axis,lean), (Vector3){0,1,0}, 0,
                 (Vector3){.24f,.3f,.18f}, WHITE);
    pb_draw_mesh(&renderer->meshes, PB_MESH_SPHERE,
                 mote_local(p,(Vector3){.23f,.18f,-.55f},yaw,lean_axis,lean), (Vector3){0,1,0}, 0,
                 (Vector3){.24f,.3f,.18f}, WHITE);
    for (i = 0; i < 5; ++i) {
        float a = (float)i*1.25663706f + (player->state==PB_PLAYER_GLIDING?elapsed*.9f:speed*0.03f);
        Vector3 q=mote_local(p,(Vector3){cosf(a)*petal_open,sinf(a)*petal_open,.18f},yaw,lean_axis,lean);
        pb_draw_mesh(&renderer->meshes, PB_MESH_PETAL, q, (Vector3){0,0,1},
                     a*57.2958f, (Vector3){1,1,1}, petal);
    }
    pb_draw_mesh(&renderer->meshes, PB_MESH_WEDGE,mote_local(p,(Vector3){-.28f,-.62f,0},yaw,lean_axis,lean),
                 (Vector3){0,1,0}, yaw*57.2958f, (Vector3){.45f,.2f,.65f}, PURPLE);
    pb_draw_mesh(&renderer->meshes, PB_MESH_WEDGE,mote_local(p,(Vector3){.28f,-.62f,0},yaw,lean_axis,lean),
                 (Vector3){0,1,0}, yaw*57.2958f, (Vector3){.45f,.2f,.65f}, PURPLE);
    for(i=1;i<=3;++i) {
        Vector3 tail={p.x-player->facing.x*(.55f+i*.28f),p.y+.1f+sinf(elapsed*6-i)*.08f,
                      p.z-player->facing.z*(.55f+i*.28f)};
        pb_draw_mesh(&renderer->meshes,PB_MESH_SPHERE,tail,(Vector3){0,1,0},0,
                     (Vector3){.2f,.12f,.2f},(Color){255,191,102,(unsigned char)(230-i*35)});
    }
    if(flow_aura&&horizontal>4) for(i=0;i<6;++i) {
        float a=elapsed*2.4f+i*1.0472f;
        Vector3 q={p.x+cosf(a)*(1.05f+horizontal*.035f),p.y+.15f+sinf(a*2)*.45f,
                   p.z+sinf(a)*(1.05f+horizontal*.035f)};
        Color glow=i&1?(Color){116,238,220,190}:(Color){255,169,105,190};
        pb_draw_mesh(&renderer->meshes,PB_MESH_PETAL,q,(Vector3){0,1,0},-a*57.2958f,
                     (Vector3){.45f,.45f,.45f},glow);
    }
}

static void start_level(int id, PbLevel *level, PbCollisionWorld *collision,
                        PbGameplay *gameplay, PbPlayer *player, PbFollowCamera *camera,
                        PbParticles *particles, bool reduced)
{
    if(id==PB_LEVEL_CASCADE) pb_level_prismrush_init(level,collision);
    else if(id==PB_LEVEL_FOUNDRY) pb_level_foundry_init(level,collision);
    else if(id==PB_LEVEL_CROWN) pb_level_crown_init(level,collision);
    else pb_level_petalgarden_init(level,collision);
    pb_gameplay_init(gameplay,level->level_id);
    pb_player_init(player,level->respawn);
    pb_camera_init(camera,player->position);
    *particles=(PbParticles){0}; particles->reduced=reduced;
    pb_audio_set_level(level->level_id);
}

int main(void)
{
    const float fixed_dt = 1.0f/120.0f;
    float accumulator = 0;
    float simulation_time = 0;
    float elapsed = 0.0f;
    float particle_clock = 0.0f;
    bool free_camera = false;
    bool jump_latched = false;
    bool burst_latched = false;
    bool reduced_effects = false;
    bool controller_prompts = false;
    bool should_exit = false;
    PbGameMode mode=PB_MODE_TITLE;
    PbGameMode options_return=PB_MODE_TITLE;
    int menu_selection=0;
    float intro_timer=0;
    float restart_hold=0;
    PbRenderer renderer = {0};
    PbParticles particles = {0};
    PbCollisionWorld collision = {0};
    PbPlayer player;
    PbFollowCamera follow;
    PbLevel level;
    PbGameplay gameplay;
    PbSaveData save;
    pb_platform_open();
    pb_audio_open();
    pb_renderer_open(&renderer);
    pb_level_petalgarden_init(&level,&collision);
    pb_gameplay_init(&gameplay,level.level_id);
    pb_save_load(&save);
    if(save.option_flags&4) ToggleBorderlessWindowed();
    pb_audio_set_volume(save.volume_master,save.volume_music,save.volume_sfx);
    pb_audio_set_level(level.level_id);
    pb_player_init(&player, level.respawn);
    pb_camera_init(&follow, player.position);

    while (pb_platform_running()) {
        PbInput input = pb_platform_input();
        PbGameMode frame_mode=mode;
        float dt = fminf(pb_platform_frame_time(), 0.05f);
        float length = sqrtf(input.move_x*input.move_x + input.move_y*input.move_y);
        Camera3D draw_camera;
        Vector3 draw_position;
        bool action=input.jump_pressed||IsKeyPressed(KEY_ENTER);
        bool back=input.menu_back_pressed;
        bool nav_up=IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W)||IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_UP);
        bool nav_down=IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S)||IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        bool nav_left=IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_A)||IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        bool nav_right=IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_D)||IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        if(input.controller_active||IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN)||
           IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_LEFT)||
           IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)||
           IsGamepadButtonPressed(0,GAMEPAD_BUTTON_MIDDLE_RIGHT)) controller_prompts=true;
        else if(input.move_x||input.move_y||input.camera_x||input.camera_y||action||back) controller_prompts=false;
        elapsed += dt;
        accumulator = fminf(accumulator+dt,.15f);
        particle_clock += dt;
        if(mode==PB_MODE_TITLE) {
            if(nav_up) menu_selection=(menu_selection+2)%3;
            if(nav_down) menu_selection=(menu_selection+1)%3;
            if(action) { pb_audio_sfx(PB_SFX_MENU_CONFIRM);
                if(menu_selection==0) { mode=PB_MODE_LEVEL_SELECT; menu_selection=0; }
                else if(menu_selection==1) { options_return=PB_MODE_TITLE; mode=PB_MODE_OPTIONS; menu_selection=0; }
                else should_exit=true;
            }
            if(back) should_exit=true;
        } else if(mode==PB_MODE_LEVEL_SELECT) {
            if(nav_left&&menu_selection%2) menu_selection--;
            if(nav_right&&menu_selection%2==0) menu_selection++;
            if(nav_up&&menu_selection>=2) menu_selection-=2;
            if(nav_down&&menu_selection<2) menu_selection+=2;
            if(action) { start_level(menu_selection,&level,&collision,&gameplay,&player,&follow,&particles,reduced_effects);
                accumulator=simulation_time=elapsed=0; intro_timer=0; mode=PB_MODE_LEVEL_INTRO; pb_audio_sfx(PB_SFX_MENU_CONFIRM); }
            if(back) { mode=PB_MODE_TITLE; menu_selection=0; }
        } else if(mode==PB_MODE_OPTIONS) {
            if(nav_up) menu_selection=(menu_selection+8)%9;
            if(nav_down) menu_selection=(menu_selection+1)%9;
            if((nav_left||nav_right)&&menu_selection<4) {
                int delta=nav_right?5:-5; uint8_t *value=menu_selection==0?&save.volume_master:
                    menu_selection==1?&save.volume_music:menu_selection==2?&save.volume_sfx:&save.camera_sensitivity;
                int adjusted=Clamp(*value+delta,0,100); *value=(uint8_t)adjusted;
                pb_audio_set_volume(save.volume_master,save.volume_music,save.volume_sfx);
            }
            if(action&&(menu_selection>=4&&menu_selection<=7)) {
                save.option_flags^=(uint8_t)(1u<<(menu_selection-4));
                reduced_effects=(save.option_flags&2)!=0; particles.reduced=reduced_effects;
                if(menu_selection==6) ToggleBorderlessWindowed();
            }
            if(back||(action&&menu_selection==8)) { pb_save_write(&save); mode=options_return; menu_selection=0; }
        } else if(mode==PB_MODE_PAUSED) {
            if(nav_up) menu_selection=(menu_selection+3)%4;
            if(nav_down) menu_selection=(menu_selection+1)%4;
            if(back||input.pause_pressed||(action&&menu_selection==0)) { mode=PB_MODE_PLAYING; menu_selection=0; }
            else if(action&&menu_selection==1) { int id=level.level_id; start_level(id,&level,&collision,&gameplay,&player,&follow,&particles,reduced_effects);
                accumulator=simulation_time=elapsed=0; intro_timer=0; mode=PB_MODE_LEVEL_INTRO; }
            else if(action&&menu_selection==2) { options_return=PB_MODE_PAUSED; mode=PB_MODE_OPTIONS; menu_selection=0; }
            else if(action&&menu_selection==3) { mode=PB_MODE_LEVEL_SELECT; menu_selection=0; }
        } else if(mode==PB_MODE_RESULTS) {
            if(nav_up) menu_selection=(menu_selection+2)%3;
            if(nav_down) menu_selection=(menu_selection+1)%3;
            if(action) { int id=menu_selection==1?(level.level_id+1)%4:level.level_id;
                if(menu_selection==2) { mode=PB_MODE_LEVEL_SELECT; menu_selection=0; }
                else { start_level(id,&level,&collision,&gameplay,&player,&follow,&particles,reduced_effects);
                    accumulator=simulation_time=elapsed=0; intro_timer=0; mode=PB_MODE_LEVEL_INTRO; }
            }
        } else if(mode==PB_MODE_PLAYING&&input.pause_pressed) { mode=PB_MODE_PAUSED; menu_selection=0; }

        if(mode==PB_MODE_PLAYING) {
            if(input.restart_down) restart_hold+=dt; else restart_hold=0;
            if(restart_hold>=.5f) { int id=level.level_id; start_level(id,&level,&collision,&gameplay,&player,&follow,&particles,reduced_effects);
                accumulator=simulation_time=elapsed=restart_hold=0; intro_timer=0; mode=PB_MODE_LEVEL_INTRO; }
            if(input.camera_recenter) { follow.yaw=atan2f(-player.facing.x,-player.facing.z); follow.manual_timer=1; }
        } else restart_hold=0;

        if(mode==PB_MODE_LEVEL_INTRO) { intro_timer+=dt; if(intro_timer>=1.5f||(action&&intro_timer>.1f)) mode=PB_MODE_PLAYING; }
        if(mode==PB_MODE_LEVEL_COMPLETE&&(level.completion_time>=1.5f||action)) {
            uint8_t seed_mask=(uint8_t)((level.seeds[0].collected?1:0)|(level.seeds[1].collected?2:0)|(level.seeds[2].collected?4:0));
            if(!gameplay.result_recorded) { pb_save_record(&save,level.level_id,pb_gameplay_result_ms(&gameplay),level.glint_count,seed_mask);
                pb_save_write(&save); gameplay.result_recorded=true; }
            mode=PB_MODE_RESULTS; menu_selection=0;
        }
        reduced_effects=(save.option_flags&2)!=0; particles.reduced=reduced_effects;
        pb_audio_set_paused(mode==PB_MODE_PAUSED||mode==PB_MODE_OPTIONS);
        input.camera_x*=.5f+save.camera_sensitivity/50.0f;
        input.camera_y*=.5f+save.camera_sensitivity/50.0f;
        if(save.option_flags&1) input.camera_y=-input.camera_y;
        pb_audio_set_chase(level.chase_active);
#if defined(POLYBLOOM_DEBUG)
        if (IsKeyPressed(KEY_F1)) {
            free_camera = !free_camera;
        }
#endif
        if (length > 1.0f) length = 1.0f;
        if(mode==PB_MODE_PLAYING||mode==PB_MODE_LEVEL_COMPLETE) {
            if (free_camera) UpdateCamera(&follow.view,CAMERA_FREE);
            else {
                if(level.chase_active) follow.yaw+=(0-follow.yaw)*fminf(dt*2.5f,1);
                pb_camera_update(&follow,player.position,player.velocity,&collision,input,dt);
            }
        }
        if(frame_mode==PB_MODE_PLAYING&&mode==PB_MODE_PLAYING) {
            jump_latched|=input.jump_pressed;
            burst_latched|=input.burst_pressed;
        } else { jump_latched=false; burst_latched=false; }
        if(mode!=PB_MODE_PLAYING&&mode!=PB_MODE_LEVEL_COMPLETE) accumulator=0;
        while (accumulator >= fixed_dt) {
            input.jump_pressed = jump_latched;
            input.burst_pressed = burst_latched;
            simulation_time += fixed_dt;
            if(mode==PB_MODE_PLAYING||mode==PB_MODE_LEVEL_COMPLETE) pb_collision_update(&collision,simulation_time);
            if (mode==PB_MODE_PLAYING&&!level.complete) {
                int old_glints=level.glint_count;
                int old_seeds=level.seed_count;
                Vector3 old_respawn=level.respawn;
                bool old_complete=level.complete;
                PbPlayerState old_state=player.state;
                pb_player_update(&player,&collision,input,follow.yaw,fixed_dt);
                if(player.state==PB_PLAYER_BURST&&old_state!=PB_PLAYER_BURST) pb_audio_sfx(PB_SFX_BURST);
                else if(player.state==PB_PLAYER_GLIDING&&old_state!=PB_PLAYER_GLIDING) pb_audio_sfx(PB_SFX_GLIDE_OPEN);
                else if(player.state==PB_PLAYER_RISING&&old_state==PB_PLAYER_GROUNDED) pb_audio_sfx(PB_SFX_JUMP);
                else if(player.state==PB_PLAYER_BOUNCED&&old_state!=PB_PLAYER_BOUNCED) pb_audio_sfx(PB_SFX_BOUNCE);
                pb_level_update(&level,&collision,player.position,fixed_dt);
                if(level.glint_count>old_glints||level.seed_count>old_seeds) {
                    int sparkle;
                    Color color=level.seed_count>old_seeds?(Color){255,201,62,255}:(Color){210,245,255,255};
                    for(sparkle=0;sparkle<6;++sparkle)
                        pb_particles_emit(&particles,player.position,
                                          (Vector3){(sparkle-2.5f)*.45f,1.2f+sparkle*.15f,((sparkle*3)%5-2)*.35f},
                                          color,.75f,.11f);
                    pb_audio_sfx(level.seed_count>old_seeds?PB_SFX_SEED:PB_SFX_GLINT);
                }
                if(Vector3Distance(old_respawn,level.respawn)>.1f) pb_audio_sfx(PB_SFX_CHECKPOINT);
                if(!old_complete&&level.complete) { pb_audio_sfx(PB_SFX_GATE); pb_audio_sfx(PB_SFX_LEVEL_COMPLETE); mode=PB_MODE_LEVEL_COMPLETE; }
                pb_gameplay_update(&gameplay,&player,level.respawn,fixed_dt,simulation_time);
                if(level.level_id==PB_LEVEL_CROWN&&!gameplay.boss_active&&gameplay.boss_health<=0)
                    level.gate_locked=false;
                if(old_state!=PB_PLAYER_GROUNDED&&player.state==PB_PLAYER_GROUNDED) {
                    int puff;
                    for(puff=0;puff<(reduced_effects?3:8);++puff)
                        pb_particles_emit(&particles,(Vector3){player.position.x,player.position.y-.6f,player.position.z},
                                          (Vector3){(puff-3.5f)*.25f,.5f,(puff%3-1)*.3f},
                                          (Color){255,231,190,220},.45f,.09f);
                    pb_audio_sfx(PB_SFX_LANDING);
                }
                if(gameplay.effect_type!=PB_EFFECT_NONE) {
                    int fx;
                    Color color=gameplay.effect_type==PB_EFFECT_DAMAGE?(Color){255,75,115,255}:
                                gameplay.effect_type==PB_EFFECT_BREAK?(Color){255,164,202,255}:(Color){255,220,90,255};
                    for(fx=0;fx<(reduced_effects?5:14);++fx)
                        pb_particles_emit(&particles,gameplay.effect_position,
                                          (Vector3){(fx%5-2)*1.1f,1+fx*.15f,((fx*3)%7-3)*.8f},color,.8f,.13f);
                    pb_audio_sfx(gameplay.effect_type==PB_EFFECT_DAMAGE?PB_SFX_DAMAGE:
                                 gameplay.effect_type==PB_EFFECT_BREAK?PB_SFX_PRISM_BREAK:PB_SFX_ENEMY_DEFEAT);
                    gameplay.effect_type=PB_EFFECT_NONE;
                }
                if(level.chase_hit) { pb_gameplay_chase_hit(&gameplay,&player,level.respawn); level.chase_hit=false; }
                if (player.position.y < -8) { pb_audio_sfx(PB_SFX_FALL); pb_gameplay_fall(&gameplay,&player,level.respawn); }
            } else if(mode==PB_MODE_LEVEL_COMPLETE) level.completion_time+=fixed_dt;
            jump_latched = false;
            burst_latched = false;
            input.jump_pressed = false;
            input.burst_pressed = false;
            accumulator -= fixed_dt;
        }
        draw_position = pb_player_interpolated(&player,accumulator/fixed_dt);
        follow.distance += (pb_level_camera_distance(&level,draw_position)-follow.distance)*fminf(dt*3,1);
        draw_camera = follow.view;
        if (mode==PB_MODE_PLAYING&&length > .7f && particle_clock > .045f) {
            Vector3 v = {-input.move_x*.5f, .45f, input.move_y*.5f};
            pb_particles_emit(&particles, (Vector3){draw_position.x,draw_position.y-.55f,draw_position.z},
                              v, (Color){255,224,148,255}, .65f, .08f);
            particle_clock = 0;
        }
        if(mode==PB_MODE_PLAYING&&player.state==PB_PLAYER_BURST&&particle_clock>.018f) {
            pb_particles_emit(&particles,draw_position,Vector3Scale(player.facing,-1.5f),
                              (Color){255,121,191,235},.45f,.16f);
            particle_clock=0;
        }
        pb_particles_update(&particles, dt);
        if (should_exit) break;

        pb_renderer_begin(&renderer,draw_camera,
                          level.level_id==PB_LEVEL_CASCADE?(Color){47,31,86,255}:
                          level.level_id==PB_LEVEL_FOUNDRY?(Color){69,29,74,255}:
                          level.level_id==PB_LEVEL_CROWN?(Color){29,38,82,255}:(Color){255,240,207,255});
        pb_draw_world(&renderer, &particles, draw_position, elapsed,level.level_id,reduced_effects);
        pb_level_draw(&level,&renderer,simulation_time,reduced_effects);
        pb_gameplay_draw(&gameplay,&renderer,simulation_time);
        draw_mote(&renderer,draw_position,&player,elapsed,(save.option_flags&8)==0);
        pb_renderer_end(&renderer);
        if(mode==PB_MODE_PLAYING||mode==PB_MODE_LEVEL_COMPLETE) {
            DrawText(TextFormat("%s   GLINTS %02d/30   SEEDS %d/3",pb_level_section_name(&level),level.glint_count,level.seed_count),36,32,18,DARKPURPLE);
            DrawText(TextFormat("HEALTH %s%s%s   TIME %.2f",gameplay.health>0?"*":"",gameplay.health>1?"*":"",gameplay.health>2?"*":"",
                                pb_gameplay_result_ms(&gameplay)/1000.0f),36,58,18,DARKPURPLE);
            if(level.level_id==PB_LEVEL_CROWN&&gameplay.boss_active)
                DrawText(TextFormat("PRISM SOVEREIGN  %d / 5",gameplay.boss_health),
                         GetScreenWidth()/2-118,32,20,(Color){255,115,191,255});
            if(mode==PB_MODE_PLAYING&&level.section==PB_SECTION_AWAKENING&&gameplay.run_time<8)
                DrawText(controller_prompts?"LEFT STICK / D-PAD MOVE   RIGHT STICK LOOK   A JUMP   X BURST":
                                            "WASD / ARROWS MOVE   AUTO CAMERA   SPACE JUMP   SHIFT BURST",
                         36,86,16,(Color){75,57,102,230});
        }
        if(mode==PB_MODE_TITLE) pb_ui_title(menu_selection,controller_prompts);
        else if(mode==PB_MODE_LEVEL_SELECT) pb_ui_level_select(menu_selection,&save,controller_prompts);
        else if(mode==PB_MODE_LEVEL_INTRO) pb_ui_intro(&level,intro_timer);
        else if(mode==PB_MODE_PAUSED) pb_ui_pause(menu_selection,controller_prompts);
        else if(mode==PB_MODE_OPTIONS) pb_ui_options(menu_selection,&save,controller_prompts);
        else if(mode==PB_MODE_RESULTS) pb_ui_results(&level,&gameplay,menu_selection,controller_prompts);
#if defined(POLYBLOOM_DEBUG)
        if(mode==PB_MODE_PLAYING||mode==PB_MODE_LEVEL_COMPLETE) {
            DrawFPS(GetScreenWidth() - 100, 20);
            DrawText(TextFormat("%s  VEL %.1f %.1f %.1f  P:%d  F1 FREECAM %s",
                                pb_player_state_name(player.state),player.velocity.x,player.velocity.y,
                                player.velocity.z,particles.active,free_camera?"ON":"OFF"),
                     36,GetScreenHeight()-42,16,DARKGRAY);
        }
#endif
        EndDrawing();
    }
    if (free_camera) EnableCursor();
    pb_renderer_close(&renderer);
    pb_audio_close();
    pb_platform_close();
    return 0;
}
