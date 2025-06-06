/*
 * File: pols voice.c
 * Overlay: ovl_Pols Voice
 * Description: Pols Voice enemy (weak to only arrows) from the Hylian Modding actor pack.
 */

#include "pols_voice.h"
#include "assets/objects/hm_pack/object_pols_voice/object_pols_voice.h"

#include "libc64/qrand.h"
#include "attributes.h"
#include "gfx.h"
#include "gfx_setupdl.h"
#include "ichain.h"
#include "rand.h"
#include "segmented_address.h"
#include "sfx.h"
#include "sys_matrix.h"
#include "z_lib.h"
#include "z64draw.h"
#include "z64effect.h"
#include "z_en_item00.h"
#include "z64item.h"
#include "z64play.h"
#include "z64player.h"
#include "z64save.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void PolsVoice_Init(Actor* thisx, PlayState* play);
void PolsVoice_Destroy(Actor* thisx, PlayState* play);
void PolsVoice_Update(Actor* thisx, PlayState* play);
void PolsVoice_Draw(Actor* thisx, PlayState* play);

void PolsVoice_Idle(PolsVoice* this, PlayState* play);
void PolsVoice_Hop(PolsVoice* this, PlayState* play);
void PolsVoice_StartGrab(PolsVoice* this, PlayState* play);
void PolsVoice_Grab(PolsVoice* this, PlayState* play);
void PolsVoice_Gnaw(PolsVoice* this, PlayState* play);
void PolsVoice_EndGnaw(PolsVoice* this, PlayState* play);
void PolsVoice_Damaged(PolsVoice* this, PlayState* play);
void PolsVoice_Stunned(PolsVoice* this, PlayState* play);
void PolsVoice_Die(PolsVoice* this, PlayState* play);

typedef enum {
    POLSVOICE_DMGEFF_NONE,
    POLSVOICE_DMGEFF_STUN,
    POLSVOICE_DMGEFF_DEFAULT,
} PolsVoiceDamageEffect;

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, POLSVOICE_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(3, POLSVOICE_DMGEFF_DEFAULT),
    /* Slingshot     */ DMG_ENTRY(2, POLSVOICE_DMGEFF_DEFAULT),
    /* Explosive     */ DMG_ENTRY(4, POLSVOICE_DMGEFF_DEFAULT),
    /* Boomerang     */ DMG_ENTRY(0, POLSVOICE_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Hammer swing  */ DMG_ENTRY(3, POLSVOICE_DMGEFF_DEFAULT),
    /* Hookshot      */ DMG_ENTRY(0, POLSVOICE_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(2, POLSVOICE_DMGEFF_DEFAULT),
    /* Master sword  */ DMG_ENTRY(3, POLSVOICE_DMGEFF_DEFAULT),
    /* Giant's Knife */ DMG_ENTRY(4, POLSVOICE_DMGEFF_DEFAULT),
    /* Fire arrow    */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Ice arrow     */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Light arrow   */ DMG_ENTRY(10, POLSVOICE_DMGEFF_DEFAULT),
    /* Unk arrow 1   */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Unk arrow 2   */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Unk arrow 3   */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Fire magic    */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Ice magic     */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Light magic   */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Shield        */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, POLSVOICE_DMGEFF_DEFAULT),
    /* Giant spin    */ DMG_ENTRY(4, POLSVOICE_DMGEFF_DEFAULT),
    /* Master spin   */ DMG_ENTRY(3, POLSVOICE_DMGEFF_DEFAULT),
    /* Kokiri jump   */ DMG_ENTRY(3, POLSVOICE_DMGEFF_DEFAULT),
    /* Giant jump    */ DMG_ENTRY(5, POLSVOICE_DMGEFF_DEFAULT),
    /* Master jump   */ DMG_ENTRY(4, POLSVOICE_DMGEFF_DEFAULT),
    /* Unknown 1     */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, POLSVOICE_DMGEFF_DEFAULT),
    /* Unknown 2     */ DMG_ENTRY(0, POLSVOICE_DMGEFF_NONE),
};

const ActorProfile Pols_Voice_Profile = {
    ACTOR_POLS_VOICE,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_POLS_VOICE,
    sizeof(PolsVoice),
    (ActorFunc)PolsVoice_Init,
    (ActorFunc)PolsVoice_Destroy,
    (ActorFunc)PolsVoice_Update,
    (ActorFunc)PolsVoice_Draw,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COL_MATERIAL_HIT5,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xFFCFFFFF, 0x08, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 40, 75, 0, { 0, 0, 0 } },
};

void PolsVoice_Init(Actor* thisx, PlayState* play) {
    PolsVoice* this = (PolsVoice*)thisx;

    Actor_SetScale(&this->actor, 0.015f);

    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.colChkInfo.health = 10;
    this->actor.gravity = -1.0f;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 45.0f);
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 60.0f, 60.0f,
                            UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 |
                                UPDBGCHECKINFO_FLAG_4);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    SkelAnime_InitFlex(play, &this->skelAnime, &gPolsVoiceSkel, &gPolsVoiceSkelIdleAnim, this->jointTable,
                       this->morphTable, GPOLSVOICESKEL_NUM_LIMBS);

    this->actor.colChkInfo.damageTable = &sDamageTable;

    this->actionFunc = PolsVoice_Idle;
}

void PolsVoice_Destroy(Actor* thisx, PlayState* play) {
    PolsVoice* this = (PolsVoice*)thisx;
}

// HELPERS

void PolsVoice_RotateTowardPoint(PolsVoice* this, Vec3f* point, s16 step) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardPoint(&this->actor, point), 3, step, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
}

// SETUP FUNCTIONS

void PolsVoice_SetupIdle(PolsVoice* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    Animation_MorphToLoop(&this->skelAnime, &gPolsVoiceSkelIdleAnim, -6.0f);
    this->actionFunc = PolsVoice_Idle;
}

void PolsVoice_SetupHop(PolsVoice* this, PlayState* play) {
    Animation_MorphToLoop(&this->skelAnime, &gPolsVoiceSkelHopAnim, 4.0f);
    this->actionFunc = PolsVoice_Hop;
}

void PolsVoice_SetupStartGrab(PolsVoice* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    Animation_MorphToPlayOnce(&this->skelAnime, &gPolsVoiceSkelGrabAnim, -3.0f);
    this->actionFunc = PolsVoice_StartGrab;
}

void PolsVoice_SetupGrab(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.speed = 10.0f;
    this->actor.velocity.y = 10.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->actionFunc = PolsVoice_Grab;
}

void PolsVoice_SetupGnaw(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = 0.0f;
    this->actor.shape.rot.x = DEG_TO_BINANG(-25.0f);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Animation_Change(&this->skelAnime, &gPolsVoiceSkelGrabAnim, 1.0f, 0.0f, 12.0f, ANIMMODE_ONCE, 0.0f);
    this->actionFunc = PolsVoice_Gnaw;
}

void PolsVoice_SetupEndGnaw(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->gnawTimer = 0;
    this->actor.speed = 8.0f;
    this->actor.velocity.y = 4.0f;
    this->actor.gravity = -1.0f;
    this->actor.shape.rot.x = 0.0f;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    Animation_MorphToPlayOnce(&this->skelAnime, &gPolsVoiceSkelHopAnim, -4.0f);
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->actionFunc = PolsVoice_EndGnaw;
}

void PolsVoice_SetupDamaged(PolsVoice* this, PlayState* play) {
    this->actor.speed = -4.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    Animation_MorphToPlayOnce(&this->skelAnime, &gPolsVoiceSkelDamagedAnim, -3.0f);
    this->actionFunc = PolsVoice_Damaged;
}

void PolsVoice_SetupStunned(PolsVoice* this, PlayState* play) {
    this->actor.speed = 0.0f;
    Animation_Change(&this->skelAnime, &gPolsVoiceSkelDamagedAnim, 0.0f, 2.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
    this->actionFunc = PolsVoice_Stunned;
}

void PolsVoice_SetupDie(PolsVoice* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.world.rot.y = this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    Animation_MorphToPlayOnce(&this->skelAnime, &gPolsVoiceSkelDieAnim, -3.0f);
    this->actionFunc = PolsVoice_Die;
}

// MAIN

#define POLSVOICE_NOTICE_RADIUS 450.0f

void PolsVoice_Idle(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 playerAlreadyGrabbed = player->stateFlags2 & PLAYER_STATE2_7;

    SkelAnime_Update(&this->skelAnime);

    if (this->actor.xyzDistToPlayerSq < SQ(POLSVOICE_NOTICE_RADIUS) && this->actor.bgCheckFlags & BGCHECKFLAG_GROUND &&
        !playerAlreadyGrabbed) {
        PolsVoice_SetupHop(this, play);
    }
}

#define POLSVOICE_GRAB_JUMP_RADIUS 250.0f

void PolsVoice_Hop(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 playerAlreadyGrabbed = player->stateFlags2 & PLAYER_STATE2_7;
    u8 readyToGrab = Actor_IsFacingAndNearPlayer(&this->actor, POLSVOICE_GRAB_JUMP_RADIUS, DEG_TO_BINANG(35.0f)) &&
                     this->actor.bgCheckFlags & BGCHECKFLAG_GROUND && !playerAlreadyGrabbed;

    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.1f, 1.0f, 0.0f);

    DECR(this->aggroTimer);

    if (this->skelAnime.curFrame == 3.0f) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
        this->actor.speed += 5.0f;
    } else if (this->skelAnime.curFrame == 0.0f) {
        if (!this->aggroTimer && this->actor.xyzDistToPlayerSq > SQ(POLSVOICE_NOTICE_RADIUS) ||
            !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || playerAlreadyGrabbed) {
            this->actionFunc = PolsVoice_SetupIdle;
            return;
        }
        if (readyToGrab) {
            this->aggroTimer = 0;
            PolsVoice_SetupStartGrab(this, play);
        }
    }

    PolsVoice_RotateTowardPoint(this, &player->actor.world.pos, DEG_TO_BINANG(10.0f));
}

void PolsVoice_StartGrab(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);

    if (this->skelAnime.curFrame == 12.0f) {
        PolsVoice_SetupGrab(this, play);
    }
}

#define POLSVOICE_PLAYER_HEAD_GRAB_RADIUS 45.0f

void PolsVoice_Grab(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 playerAlreadyGrabbed = player->stateFlags2 & PLAYER_STATE2_7;

    SkelAnime_Update(&this->skelAnime);

    if ((this->skelAnime.curFrame >= 20.0f && this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || playerAlreadyGrabbed) {
        PolsVoice_SetupIdle(this, play);
        return;
    }

    if (Actor_WorldDistXYZToPoint(&this->actor, &player->bodyPartsPos[PLAYER_BODYPART_COLLAR]) <
        POLSVOICE_PLAYER_HEAD_GRAB_RADIUS) {
        if (play->grabPlayer(play, player)) {
            this->isGnawing = true;
            PolsVoice_SetupGnaw(this, play);
        }
    }
}

void PolsVoice_Gnaw(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.world.pos = player->bodyPartsPos[PLAYER_BODYPART_HEAD];
    this->actor.world.pos.y -= 8.0f;

    if (SkelAnime_Update(&this->skelAnime)) {
        Animation_Change(&this->skelAnime, &gPolsVoiceSkelGrabAnim, 1.0f, 0.0f, 12.0f, ANIMMODE_ONCE, 0.0f);
    }

    if (!(player->stateFlags2 & PLAYER_STATE2_7)) {
        PolsVoice_SetupEndGnaw(this, play);
    }

    if (DECR(this->gnawTimer) == 0) {
        if (!LINK_IS_ADULT) {
            Player_PlaySfx(player, NA_SE_VO_LI_DAMAGE_S_KID);
        } else {
            Player_PlaySfx(player, NA_SE_VO_LI_DAMAGE_S);
        }
        play->damagePlayer(play, -8);
        this->gnawTimer = 20;
    }

    if (this->gnawTimer == 10) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DEADHAND_BITE);
    }
}

void PolsVoice_EndGnaw(PolsVoice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.1f, 1.0f, 0.0f);

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->isGnawing = false;
        PolsVoice_SetupHop(this, play);
        return;
    }

    if (this->actor.xzDistToPlayer > 30.0f) {
        this->isGnawing = false;
    }
}

void PolsVoice_Damaged(PolsVoice* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 3.0f, 0.5f, 0.0f);

    if (SkelAnime_Update(&this->skelAnime)) {
        this->aggroTimer = 10 * 20;
        PolsVoice_SetupHop(this, play);
    }
}

void PolsVoice_Stunned(PolsVoice* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.colorFilterTimer == 0) {
        if (this->actor.colChkInfo.health == 0) {
            PolsVoice_SetupDie(this, play);
        } else {
            PolsVoice_SetupHop(this, play);
        }
    }
}

void PolsVoice_Die(PolsVoice* this, PlayState* play) {
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f effectVel = { 0.0f, 4.0f, 0.0f };
    Vec3f effectPos = this->actor.world.pos;

    if (SkelAnime_Update(&this->skelAnime) || this->drowned) {
        Actor_SetScale(&this->actor, this->actor.scale.x * 0.8f);

        if (this->actor.scale.x <= 0.001f) {
            effectPos.y += 10.0f;
            EffectSsDeadDb_Spawn(play, &effectPos, &effectVel, &zeroVec, 90, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9,
                                 true);
            Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RECOVERY_HEART);
            Actor_Kill(&this->actor);
            if (play->msgCtx.ocarinaAction == OCARINA_ACTION_FREE_PLAY_DONE) {
                play->msgCtx.ocarinaAction = 0xFFFF;
            }
        }
    }
}

void PolsVoice_CheckDrowned(PolsVoice* this, PlayState* play) {
    if (!this->drowned && (this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && (this->actor.depthInWater > 5.0f)) {
        Actor_SetDropFlag(&this->actor, &this->collider.elem, true);
        Actor_PlaySfx(&this->actor, NA_SE_EN_DEADHAND_DEAD);
        Enemy_StartFinishingBlow(play, &this->actor);
        this->drowned = true;
        this->actor.gravity = -0.1f;
        this->actionFunc = PolsVoice_SetupDie;
        return;
    }
}

void PolsVoice_CheckDamage(PolsVoice* this, PlayState* play) {
    PolsVoice_CheckDrowned(this, play);

    if (!this->drowned && this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (this->invincibilityTimer == 0) {
            this->invincibilityTimer = 40;
        }
        Actor_SetDropFlag(&this->actor, &this->collider.elem, true);

        if (this->actionFunc != PolsVoice_Die && this->actionFunc != PolsVoice_Damaged && !this->isGnawing) {
            switch (this->actor.colChkInfo.damageEffect) {
                case POLSVOICE_DMGEFF_STUN:
                    if (this->actor.colChkInfo.health > 1) {
                        Actor_PlaySfx(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
                        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA,
                                             80);
                        this->actionFunc = PolsVoice_SetupStunned;
                        break;
                    }
                    break;
                case POLSVOICE_DMGEFF_DEFAULT:
                    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, 20);
                    Actor_ApplyDamage(&this->actor);
                    if (this->actor.colChkInfo.health == 0) {
                        Actor_PlaySfx(&this->actor, NA_SE_EN_DEADHAND_DEAD);
                        Enemy_StartFinishingBlow(play, &this->actor);
                        this->actionFunc = PolsVoice_SetupDie;
                    } else {
                        Actor_PlaySfx(&this->actor, NA_SE_EN_DEADHAND_DAMAGE);
                        this->actionFunc = PolsVoice_SetupDamaged;
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

void PolsVoice_Update(Actor* thisx, PlayState* play) {
    PolsVoice* this = (PolsVoice*)thisx;

    DECR(this->invincibilityTimer);

    PolsVoice_CheckDamage(this, play);
    this->actionFunc(this, play);

    Collider_UpdateCylinder(&this->actor, &this->collider);

    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 60.0f, 60.0f,
                            UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 |
                                UPDBGCHECKINFO_FLAG_4);

    if (!this->isGnawing) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }

    if (!this->invincibilityTimer && this->actionFunc != PolsVoice_Damaged && this->actionFunc != PolsVoice_Die) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    // Check for any Ocarina song to kill them
    if (play->msgCtx.ocarinaAction == OCARINA_ACTION_FREE_PLAY_DONE) {
        if (this->actor.xzDistToPlayer <= POLSVOICE_NOTICE_RADIUS) {
            if (this->actionFunc != PolsVoice_Die) {
                Actor_PlaySfx(&this->actor, NA_SE_EN_DEADHAND_DEAD);
                Enemy_StartFinishingBlow(play, &this->actor);
                this->actionFunc = PolsVoice_SetupDie;
            }
        } else {
            play->msgCtx.ocarinaAction = 0xFFFF;
        }
    }
}

void PolsVoice_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    PolsVoice* this = (PolsVoice*)thisx;

    if (limbIndex == GPOLSVOICESKEL_HEAD_LIMB) {
        Vec3f src = { 0.0f, 10.0f, 0.0f };
        Vec3f dest;

        Matrix_MultVec3f(&src, &dest);
        this->actor.focus.pos.x = dest.x;
        this->actor.focus.pos.y = dest.y;
        this->actor.focus.pos.z = dest.z;
    }
}

void PolsVoice_Draw(Actor* thisx, PlayState* play) {
    PolsVoice* this = (PolsVoice*)thisx;

    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          PolsVoice_PostLimbDraw, &this->actor);
}