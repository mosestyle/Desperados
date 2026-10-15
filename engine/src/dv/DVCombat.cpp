// The firearms of the humans (Game/DVElementActorHuman.cpp: Shoot, ShootSingleBullet,
// GetHitProbability; DVElementActorHuman::Translate 0x2f, a bullet hits; the Kill functions of
// DVElementActorPC / NPC / NPCVillain), and the noises the shots make (DVElementActorNPC::Noise).
#include <algorithm>
#include <cmath>
#include <string>

#include "dv/DVArmsTrader.h"
#include "dv/DVArtificialIntelligence.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVScript.h"
#include "dv/DVSequence.h"
#include "dv/DVSprite.h"
#include "sb/SBFile.h"

extern int desprand();

// the heroes' firearm and life (DVCooper::DVCooper and the others; DVElementActorPC: 100)
void DVElementActor::InitFirearm() {
    if (!IsHero()) return;
    DVPsychoanalyst* psy = DVPsychoanalyst::mpTheOnlyPsychoanalistInTown;
    DVArmsTrader* arms = DVArmsTrader::mpTheOnlyArmstraderInTown;
    heroProfile = psy ? psy->GetCharacterFromCompendium(HeroProfileId((int)kind)) : nullptr;
    firearm = heroProfile && arms ? arms->GetFirearmFromCatalog(heroProfile->firearm) : nullptr;
    ammo = firearm ? (int16_t)firearm->magazine : 0;
    life = 100;
}

uint8_t DVElementActor::GetShootingSkill() const {
    if (IsHero()) return heroProfile ? heroProfile->properties[6] : 50;  // DVElementActorPC
    if (kind == KIND_VILLAIN) {
        // DVElementActorNPCVillain: less when drunk
        uint32_t s = villainSkill;
        if (ai && ai->drunk) s = (uint32_t)(((float)ai->drunk * -0.01f + 1.0f) * (float)s);
        return (uint8_t)s;
    }
    return 100;  // DVElementActorNPC
}

// the height the shots start from (DVElementActorHuman::Shoot)
SBGeoPoint3D DVElementActor::FirearmPoint() const {
    SBGeoPoint3D p = sprite->pos.pos3D;
    switch (sprite->pos.posture) {
    case 1: p.z += 46.0f; break;
    case 2: p.z += 2.0f; break;
    case 0xd: p.z += 20.0f; break;
    default: p.z += 32.0f; break;
    }
    return p;
}

uint8_t DVElementActor::GetHitProbability(float ratio, bool hard, bool special) const {
    if (IsSnipering(special)) return 100;
    DVEngine* e = DVEngine::mpEngine;
    if (e && e->night) ratio += ratio;
    uint32_t r = (uint32_t)(int)(ratio * 100.0f) & 0xffff;
    if (r > 100) r = 100;
    // the chances at the start and the end of each fifth of the range, for a good shooter (hi)
    // and a bad one (lo); `hard`: a target lying down (or the like) is harder to hit
    int lo0, hi0, lo1, hi1;  // local_14 / iVar9 at the start, iVar7 / iVar8 at the end
    if (r < 0x32) {
        if (r < 0x19) {
            int a = (!hard ? 5 : 0) + 0x5a;
            hi0 = 100;
            if (r < 0xc) {
                lo0 = 100;
                hi1 = 100;
                lo1 = a;
            } else {
                lo1 = hard ? 0x50 : 0x5a;
                r -= 0xc;
                hi1 = 100;
                lo0 = a;
            }
        } else {
            if (r < 0x25) {
                lo0 = hard ? 0x50 : 0x5a;
                lo1 = hard ? 0x32 : 0x46;
                r -= 0x19;
            } else {
                lo0 = hard ? 0x32 : 0x46;
                lo1 = hard ? 0x14 : 0x28;
                r -= 0x25;
            }
            hi0 = 100;
            hi1 = 100;
        }
    } else if (r < 0x4b) {
        if (r < 0x3e) {
            lo0 = hard ? 0x14 : 0x28;
            lo1 = hard ? 10 : 0x14;
            hi1 = hard ? 0x5a : 100;
            hi0 = 100;
            r -= 0x32;
        } else {
            lo0 = hard ? 10 : 0x14;
            lo1 = hard ? 5 : 10;
            hi0 = hard ? 0x5a : 100;
            hi1 = hard ? 0x50 : 100;
            r -= 0x3e;
        }
    } else if (r < 0x57) {
        lo0 = hard ? 5 : 10;
        hi0 = hard ? 0x50 : 100;
        hi1 = hard ? 0x32 : 0x5a;
        r -= 0x4b;
        lo1 = 0;
    } else {
        hi0 = hard ? 0x32 : 0x5a;
        r -= 0x57;
        lo0 = 0;
        lo1 = 0;
        hi1 = 0;
    }
    float skill = (float)GetShootingSkill();
    uint32_t start = (uint32_t)((float)(hi0 - lo0) * skill * 0.01f + (float)lo0) & 0xff;
    uint32_t end = (uint32_t)((float)(hi1 - lo1) * skill * 0.01f + (float)lo1) & 0xff;
    uint32_t k = (r & 0xffff) == 0xd ? 0xc : (r & 0xffff);
    uint32_t p = (uint32_t)((double)(int)(k * (end - start)) * 0.083333 + (double)start);
    return (uint8_t)p;
}

// the target's difficulty by posture (the table at 0x0855d7c2): lying, crouching ... are hard
static bool HardPosture(uint32_t posture) {
    static const uint8_t k[9] = {1, 1, 0, 1, 0, 1, 0, 0, 1};
    return posture >= 2 && posture <= 10 ? k[posture - 2] != 0 : false;
}

// a bullet that does not hit anyone: the original throws it to where it lands (dust, the
// animals' alert); here it only goes
bool DVElementActor::ShootSingleBullet(const SBGeoPoint3D& at, const DVFirearm* w, bool special, bool place) {
    (void)at;
    (void)w;
    (void)special;
    (void)place;
    return false;
}

bool DVElementActor::Shoot(DVElement* t, const DVFirearm* w, bool special) {
    if (DVSequenceManager::trace) SBLog("%s #%d (out %d, state %u/%u, locked %d) shoots at %s #%d", KindName(), DVScript::Handle(this), knockedOut, ai ? ai->state : 0, ai ? ai->substate : 0, ai && ai->locked, t ? t->KindName() : "-", DVScript::Handle(t));
    if (!w || !t || !t->sprite) return false;
    SBGeoPoint3D from = FirearmPoint();
    DVElementActor* target = dynamic_cast<DVElementActor*>(t);
    bool canHit = false;
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    DVEngine* e = DVEngine::mpEngine;
    if (target && (target->IsHero() || target->IsNPC())) {
        bool hard = HardPosture(target->sprite->pos.posture);
        SBGeoPoint3D a, b, c;
        target->DetectionPoints(a, b, c);
        SBGeoVector3D d(b.x - from.x, (b.y - from.y) * 1.33f, b.z - from.z);
        float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (dist < (float)w->range) {
            // one of its points must be in sight of the gun
            canHit = !grid || grid->IsReachable(from, b, false) || grid->IsReachable(from, a, false) ||
                     grid->IsReachable(from, c, false);
            if (canHit && e && e->night) {
                float r = (float)(DVArtificialIntelligence::muwStandardViewPolygonRadius >> 1);
                canHit = dist * dist <= r * r;
            }
        }
        if (kind == KIND_VILLAIN) {
            // a villain shoots no farther than he sees
            canHit = canHit && dist <= (float)view.length;
            lastShotHit = canHit;
        }
        uint16_t damage = 0;
        for (uint16_t i = 0; i < w->bullets; ++i) {
            if (!canHit) {
                ShootSingleBullet(b, w, special, false);
                continue;
            }
            uint8_t p = GetHitProbability(dist / (float)w->range, hard, special);
            int roll = desprand() % 100;
            if ((int)p <= roll) {
                ShootSingleBullet(b, w, special, false);
                continue;
            }
            // the damage at the distance of the target's feet
            const SBGeoPoint3D& tp = target->sprite->pos.pos3D;
            SBGeoVector3D q(tp.x - from.x, (tp.y - from.y) * 1.33f, tp.z - from.z);
            uint32_t dd = (uint32_t)std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
            uint16_t dm = w->CalculateRandomBulletDamage(w->range, dd);
            damage = (uint16_t)std::min<uint32_t>(0xfd65, (uint32_t)damage + dm);
            if (dm == 12345) damage = 12345;
        }
        if (damage) target->GotBullet(damage, this);
    }
    return canHit;
}

// DVElementActorHuman::Translate, the physics operation 0x2f: the bullets' damage, then the hit
// (0x19 standing, 0x1a lying) or the death (0x21)
void DVElementActor::GotBullet(uint16_t damage, DVElement* shooter) {
    if (dead || !active) return;
    DVPositionInterface& pos = sprite->pos;
    if (shooter && shooter->sprite) {
        // DVElementActor::TurnSprite: he turns to the shot
        SBGeoVector2D d = shooter->sprite->pos.posMap - pos.posMap;
        if (d.x != 0.0f || d.y != 0.0f) pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 1.0f) & 0xf);
    }
    DVEngine* e = DVEngine::mpEngine;
    bool immortal = IsHero() && e && e->highlander;
    if (!immortal) {
        if (damage >= (uint16_t)std::max<int16_t>(life, 0)) life = 0;
        else life = (int16_t)(life - damage);
    }
    SBLog("%s hit (%u) by %s at %.0f,%.0f: life %d", KindName(), damage, shooter ? (std::string(shooter->KindName()) + " #" + std::to_string(DVScript::Handle(shooter)) + (dynamic_cast<DVElementActor*>(shooter) && dynamic_cast<DVElementActor*>(shooter)->ai && dynamic_cast<DVElementActor*>(shooter)->ai->locked ? " locked" : "")).c_str() : "-", shooter && shooter->sprite ? shooter->sprite->pos.posMap.x : 0.f, shooter && shooter->sprite ? shooter->sprite->pos.posMap.y : 0.f, life);
    DVOrder o;
    o.id = DVOrder::NewID();
    uint32_t p = pos.posture;
    if (life < 1) {
        o.command = CMD_DIE;
    } else if (p == POSTURE_LYING || p == 5 || p == 6) {
        o.command = CMD_HIT_LYING;
    } else {
        o.command = CMD_HIT;
    }
    if (!sprite->HasAnimation(o.command)) o.command = life < 1 ? CMD_DIE : CMD_WAIT;
    orders.clear();
    orders.push_back(o);
    newOrder = true;
    isMove = false;
    if (life < 1) dead = true;
    // the noise of the hit, and for an NPC "I got a bullet" (stimulus 0x20)
    if (e) e->Noise(0x14, pos.pos3D, this);
    if (ai && shooter) {
        DVStimulus s = DVStimulus::Of(STIM_BULLET);
        s.element = shooter;
        s.source = 3;
        if (life >= 1) ai->Think(s);
    }
    if (life < 1) Kill();
}

void DVElementActor::Kill() {
    if (ai) {
        // DVElementActorNPC::Kill: calm, no suspicion, unconscious state (0, 5), the eyes shut
        ai->SetAlertStatus(0, true);
        ai->suspect = 0;
        ai->yellow = 0;
        ai->yellowBack = 0;
        ai->SetState(0, 5);
        ai->timerOn = ai->macroTimerOn = false;
        ai->queue.clear();
        if (view.status) view.status = 8;
        DVEngine* e = DVEngine::mpEngine;
        if (e && e->watched == this) e->watched = nullptr;
    }
    if (IsHero()) {
        // DVElementActorPC::Kill: the NPCs are told (CondoleOn); the mission is lost
        DVEngine* e = DVEngine::mpEngine;
        if (e) e->HeroDied(this);
    }
}

// DVArtificialMalignity::ShootAt / the hero's shot: the orders of the "shoot at" interaction
// (DVElementActorHuman::Translate 0x32: lying 8, else 0x1c)
bool DVElementActor::OrderShoot(DVElement* t) {
    if (!t || !t->sprite || dead || !active || !firearm) return false;
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = sprite->pos.posture == POSTURE_LYING ? CMD_SHOOT_LYING : CMD_SHOOT;
    if (!sprite->HasAnimation(o.command)) return false;
    o.target = t;
    o.aim = t->sprite->pos.pos3D;
    orders.clear();
    orders.push_back(o);
    newOrder = true;
    isMove = false;
    return true;
}

void DVElementActor::Reload() {
    if (!firearm || dead) return;
    if (ammo >= (int16_t)firearm->magazine) return;
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_RELOAD;
    o.tolerance = 30;  // frames
    orders.clear();
    orders.push_back(o);
    newOrder = true;
    isMove = false;
}

// ---- blows ---------------------------------------------------------------------------------------

void DVElementActor::SetConcussion(int v) {
    bool was = knockedOut;
    if (v < 0) v = 0;
    if (v > 300) v = 300;
    concussion = (uint16_t)v;
    if (!was) {
        if (v > 0x45) knockedOut = true;
    } else if (v < 0x1e) {
        knockedOut = false;
    }
    if (was == knockedOut) return;
    if (knockedOut) {
        SBLog("%s is out cold", KindName());
        if (IsHero()) {
            // DVElementActorPC::SetConcussionOfTheBrain: he can't be used while out
            DVEngine* e = DVEngine::mpEngine;
            if (e && e->Selected() == this) e->SelectNextPlayable();
        }
    } else {
        SBLog("%s comes to", KindName());
        if (IsHero()) {
            defaultCommand = CMD_LYING;
        } else if (ai && ai->substate == 2) {
            DVStimulus s = DVStimulus::Of(0xf);
            ai->Think(s);
        }
    }
}

void DVElementActor::GotHit(uint16_t damage, DVElement* from) {
    bool was = knockedOut;
    int add = (int)((uint32_t)damage * 1000 / (uint32_t)std::max(1, (int)life + (int)damage));
    SetConcussion(concussion + add);
    if (!ai) return;
    if (!was && knockedOut) {
        DVStimulus s = DVStimulus::Of(0x11);
        ai->Think(s);
        detecting = true;
        return;
    }
    ai->target = from && (from->classFlags & 3) != 1 ? from : nullptr;
    DVStimulus s = DVStimulus::Of(0x10);
    ai->Think(s);
}

void DVElementActor::KnockHit(uint16_t damage, DVElement* from) {
    if (dead || !active) return;
    DVPositionInterface& pos = sprite->pos;
    DVEngine* e = DVEngine::mpEngine;
    bool immortal = IsHero() && e && e->highlander;
    if (!immortal) life = damage >= std::max<int16_t>(life, 0) ? 0 : (int16_t)(life - damage);
    SBLog("%s punched (%u) by %s: life %d", KindName(), damage, from ? from->KindName() : "-", life);
    if (from && from->sprite) {
        SBGeoVector2D d = from->sprite->pos.posMap - pos.posMap;
        if (d.x != 0.0f || d.y != 0.0f) pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 1.0f) & 0xf);
    }
    uint32_t p = pos.posture;
    if (p == POSTURE_LYING) {
        if (life < 1) {
            dead = true;
            Kill();
            pos.SetPosture(0xe);
        } else {
            GotHit(damage, from);
        }
        return;
    }
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = life < 1 ? CMD_DIE : CMD_KNOCKED;
    o.target = from;
    if (!sprite->HasAnimation(o.command)) o.command = CMD_WAIT;
    if (e) e->Noise(0x14, pos.pos3D, this);
    if (scriptElement) EndScriptElement(false);
    orders.clear();
    orders.push_back(o);
    newOrder = true;
    isMove = false;
    if (life < 1) {
        dead = true;
        Kill();
    } else {
        GotHit(damage, from);
    }
}
