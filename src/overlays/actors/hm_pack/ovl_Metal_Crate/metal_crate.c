/*
 * File: z_metal_crate.c
 * Overlay: ovl_Metal_Crate
 * Description: Metal crate that can be broken with hammer, from the Hylian Modding actor pack.
 * Params: Same format as Obj_Kibako2.
 */

#include "metal_crate.h"
#include "assets/objects/hm_pack/object_metal_crate/object_metal_crate.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"

#include "libc64/qrand.h"
#include "attributes.h"
#include "gfx.h"
#include "gfx_setupdl.h"
#include "ichain.h"
#include "rand.h"
#include "segmented_address.h"
#include "sfx.h"
#include "sys_matrix.h"
#include "z_en_item00.h"
#include "z_lib.h"
#include "z64draw.h"
#include "z64effect.h"
#include "z64item.h"
#include "z64play.h"
#include "z64player.h"
#include "z64save.h"

#define FLAGS (0)

void MetalCrate_Init(Actor* thisx, PlayState* play);
void MetalCrate_Destroy(Actor* thisx, PlayState* play);
void MetalCrate_Update(Actor* thisx, PlayState* play);
void MetalCrate_Draw(Actor* thisx, PlayState* play);

void MetalCrate_DoNothing(MetalCrate* this, PlayState* play);

const ActorProfile Metal_Crate_Profile = {
    ACTOR_METAL_CRATE,
    ACTORCAT_BG,
    FLAGS,
    OBJECT_METAL_CRATE,
    sizeof(MetalCrate),
    (ActorFunc)MetalCrate_Init,
    (ActorFunc)MetalCrate_Destroy,
    (ActorFunc)MetalCrate_Update,
    (ActorFunc)MetalCrate_Draw,
};

static ColliderCylinderInit sCylinderInit = {
    .base = {
        .colMaterial = COL_MATERIAL_NONE,
        .acFlags = AC_ON | AC_TYPE_PLAYER,
        .ocFlags2 = OC2_TYPE_2,
        .shape = COLSHAPE_CYLINDER,
    },
    .elem = {
        .atDmgInfo = { .dmgFlags = DMG_HAMMER },
        .acElemFlags = ACELEM_ON,
    },
    .dim = { .radius = 31, .height = 48 },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDistance, 3000, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeScale, 500, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDownward, 1000, ICHAIN_STOP),
};

void MetalCrate_Init(Actor* thisx, PlayState* play) {
    MetalCrate* this = (MetalCrate*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_ProcessInitChain(thisx, sInitChain);
    DynaPolyActor_Init(&this->dyna, 0);

    CollisionHeader_GetVirtual(&gMetalCrateCol_collisionHeader, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->dyna.actor, &sCylinderInit);
    Collider_UpdateCylinder(&this->dyna.actor, &this->collider);

    this->collectibleFlag = this->dyna.actor.home.rot.z & 0x3F;

    this->dyna.actor.home.rot.z = 0;
    this->dyna.actor.world.rot.z = 0;
    this->dyna.actor.shape.rot.z = 0;
    this->dyna.actor.world.rot.x = 0;
    this->dyna.actor.shape.rot.x = 0;

    this->actionFunc = MetalCrate_DoNothing;
}

void MetalCrate_Destroy(Actor* thisx, PlayState* play) {
    MetalCrate* this = (MetalCrate*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void MetalCrate_Update(Actor* thisx, PlayState* play) {
    MetalCrate* this = (MetalCrate*)thisx;

    this->actionFunc(this, play);
}

void MetalCrate_Draw(Actor* thisx, PlayState* play) {
    MetalCrate* this = (MetalCrate*)thisx;

    Gfx_DrawDListOpa(play, gMetalCrateDL);
}

void MetalCrate_Break(MetalCrate* this, PlayState* play) {
    Vec3f* thisPos;
    Vec3f pos;
    Vec3f velocity;
    s16 angle;
    s32 i;

    // spawn fragments
    thisPos = &this->dyna.actor.world.pos;
    for (i = 0, angle = 0; i < 0x8; i++, angle += 0x4E20) {
        f32 sn = Math_SinS(angle);
        f32 cs = Math_CosS(angle);
        f32 temp_rand;
        s32 phi_s0;

        temp_rand = Rand_ZeroOne() * 30.0f;
        pos.x = sn * temp_rand;
        pos.y = (Rand_ZeroOne() * 10.0f) + 2.0f;
        pos.z = cs * temp_rand;
        velocity.x = pos.x * 0.2f;
        velocity.y = (Rand_ZeroOne() * 10.0f) + 2.0f;
        velocity.z = pos.z * 0.2f;
        pos.x += thisPos->x;
        pos.y += thisPos->y;
        pos.z += thisPos->z;
        temp_rand = Rand_ZeroOne();
        if (temp_rand < 0.05f) {
            phi_s0 = 0x60;
        } else if (temp_rand < 0.7f) {
            phi_s0 = 0x40;
        } else {
            phi_s0 = 0x20;
        }
        EffectSsKakera_Spawn(play, &pos, &velocity, &pos, -200, phi_s0, 28, 2, 0, (Rand_ZeroOne() * 30.0f) + 5.0f, 0, 0,
                             70, KAKERA_COLOR_NONE, OBJECT_METAL_CRATE, gMetalCrateFragmentDL);
    }

    // spawn dust
    static Color_RGBA8 sPrimColor = { 200, 200, 200, 255 };
    static Color_RGBA8 sEnvColor = { 100, 100, 100, 255 };
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    s16 scale;
    velocity.x = velocity.y = velocity.z = 0.0f;

    for (i = 6; i >= 0; i--) {
        pos.x = thisPos->x + ((Rand_ZeroOne() - 0.5f) * 90.0f);
        pos.y = thisPos->y + ((Rand_ZeroOne() - 0.5f) * 90.0f);
        pos.z = thisPos->z + ((Rand_ZeroOne() - 0.5f) * 90.0f);

        scale = (s16)((Rand_ZeroOne() * 100) * 0.2f) + 100;
        EffectSsDust_Spawn(play, 4, &pos, &velocity, &accel, &sPrimColor, &sEnvColor, scale, 160, 10, 0);
    }

}

void MetalCrate_SpawnCollectible(MetalCrate* this, PlayState* play) {
    s16 itemDropped;
    s16 collectibleFlagTemp;

    collectibleFlagTemp = this->collectibleFlag;
    itemDropped = this->dyna.actor.home.rot.x;
    if (itemDropped >= 0 && itemDropped < ITEM00_MAX) {
        Item_DropCollectible(play, &this->dyna.actor.world.pos, itemDropped | (collectibleFlagTemp << 8));
    }
}

void MetalCrate_SpawnContents(MetalCrate* this, PlayState* play) {
    s16 params = this->dyna.actor.params;

    if ((params & 0x8000) == 0) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_SW, this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y,
                    this->dyna.actor.world.pos.z, 0, this->dyna.actor.shape.rot.y, 0, params | 0x8000);
    } else {
        MetalCrate_SpawnCollectible(this, play);
    }
    Actor_Kill(&this->dyna.actor);
}

void MetalCrate_DoNothing(MetalCrate* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        MetalCrate_Break(this, play);
        SfxSource_PlaySfxAtFixedWorldPos(play, &this->dyna.actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        SfxSource_PlaySfxAtFixedWorldPos(play, &this->dyna.actor.world.pos, 20, NA_SE_EV_BRIDGE_CLOSE_STOP);

        this->dyna.actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        DynaPoly_DisableCollision(play, &play->colCtx.dyna, this->dyna.bgId);
        this->dyna.actor.draw = NULL;
        this->actionFunc = MetalCrate_SpawnContents;
    } else if (this->dyna.actor.xzDistToPlayer < 600.0f) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}
