// The actors' side of the scripts' sequences: DVElementActor::Instruct / Translate for the
// commands the scripts record, and the ExecuteImmediately functions (DVElementActor 0x08210360,
// DVElementActorNPC 0x0823afb0, DVElementActorPC 0x08249090).
#include <cmath>

#include "dv/DVArmsTrader.h"
#include "dv/DVArtificialIntelligence.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVScript.h"
#include "dv/DVDoor.h"
#include "dv/DVSector.h"
#include "dv/DVSequence.h"
#include "sb/SBFile.h"

void DVElementActor::EndScriptElement(bool done) {
    DVSequenceElement* e = scriptElement;
    scriptElement = nullptr;
    if (e) e->SetState(done ? SEQ_DONE : SEQ_STOPPED);
}

// DVElementActor::Instruct (0x0820d540): the element replaces what he was doing and becomes his
// orders (Translate); it is done when they are
void DVElementActor::Instruct(DVSequenceElement* e) {
    if (dead || !sprite) {
        e->SetState(SEQ_CANCELLED);
        return;
    }
    if (DVSequenceManager::trace && e->goalArea)
        SBLog("  %s #%d goes to area %d (%s)", KindName(), DVScript::Handle(this), e->goalArea->id,
              (e->goalArea->type & SECTOR_BUILDING) ? "a house" : "outside");
    if (scriptElement && scriptElement != e) EndScriptElement(false);
    DVPositionInterface& pos = sprite->pos;
    std::deque<DVOrder> seq;
    bool move = false;
    auto anim = [&](uint32_t cmd, uint32_t a) {
        if (!sprite->HasAnimation(a)) return false;
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = cmd;
        o.anim = a;
        seq.push_back(o);
        return true;
    };
    if (e->seek && e->target && e->target->sprite) {
        // DVSequenceElement::AsSeekSequence: to where the one he seeks is now, a little before him
        SBGeoPoint2D t = e->target->sprite->pos.posMap;
        SBGeoVector2D d = t - pos.posMap;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        e->point = len > 30.0f ? t - d * (25.0f / len) : pos.posMap;
    }
    if (e->command == 0xe9) {
        // DVScript::RecordEnterGame: on the map at the first place, then the walk to the second
        active = true;
        Teleport(e->point2, e->layer, e->area2);
        if (DVScript::mpInstance) DVScript::mpInstance->CheckZones(this, false);
    }
    switch (e->command) {
    case 0xe9:
    case 0x17:  // a move (DVSequence::AppendMoveToSequence), its animation: 3 walk, 5 run, ...
    case 0x1a: {  // DVScript::RecordLeaveGame: walk / run / crawl out, then off the map
        uint32_t cmd = CMD_WALK;
        if (e->anim == 5) cmd = CMD_RUN;
        if (e->anim == 9) cmd = CMD_CRAWL;
        uint32_t movePosture = cmd == CMD_CRAWL ? POSTURE_LYING : POSTURE_STANDING;
        if (pos.posture == POSTURE_LYING && cmd != CMD_CRAWL) {
            DVOrder o;
            o.id = DVOrder::NewID();
            o.command = CMD_STAND_UP;
            seq.push_back(o);
        }
        SBGeoVector2D d = e->point - pos.posMap;
        if (std::fabs(d.x) > 2.0f || std::fabs(d.y) > 2.0f) {
            DVSector* ga = e->goalArea;
            auto* goalArea = ga && (ga->type & 3) == 3 ? static_cast<DVSectorMotionArea*>(ga) : nullptr;
            if (!BuildMove(e->point, cmd, movePosture, seq, goalArea)) {
                // no way there: the original puts him there (the script must go on)
                SBLog("script: %s can't walk to %.0f,%.0f, put there", KindName(), e->point.x, e->point.y);
                if (goalArea) Teleport(e->point, e->goalLayer, goalArea);
                else Teleport(e->point, pos.layer, pos.sector);
                if (InBuilding()) active = false;
            }
        }
        if (e->command == 0x1a) {
            DVOrder o;
            o.id = DVOrder::NewID();
            o.command = CMD_LEAVE;
            seq.push_back(o);
        }
        move = true;
        break;
    }
    case 0x1d: {  // turn to a place, or (RecordAction 4) to a direction (Translate 0x1d)
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = CMD_TURN;
        if (e->action) {
            o.direction = (uint8_t)(e->param & 0xf);
        } else {
            SBGeoVector2D v = e->point - pos.posMap;
            if (v.x == 0.0f && v.y == 0.0f) break;
            o.direction = (uint8_t)(SBGetSector0to15(v, 0.57357645f) & 0xf);
        }
        if (o.direction != pos.direction) seq.push_back(o);
        break;
    }
    case 0x20:  // DVScript::RecordPlayAnim
        anim(CMD_SCRIPT_ANIM, (uint32_t)e->param);
        break;
    case 0x21:  // RecordPlayAnimLoop
        anim(CMD_SCRIPT_LOOP, (uint32_t)e->param);
        break;
    case 0x22:  // RecordPlayAnimFreeze
        anim(CMD_SCRIPT_FREEZE, (uint32_t)e->param);
        break;
    case 0x32: {  // RecordFireAt / RecordFireLocation
        uint32_t c = pos.posture == POSTURE_LYING ? CMD_SHOOT_LYING : CMD_SHOOT;
        if (!firearm || !sprite->HasAnimation(c)) break;
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = c;
        if (e->target && e->target->sprite) {
            o.target = e->target;
            o.aim = e->target->sprite->pos.pos3D;
        } else {
            o.aim = SBGeoPoint3D(e->point.x, e->point.y, 0.0f);
        }
        if (ammo < 1) ammo = (int16_t)firearm->magazine;
        seq.push_back(o);
        break;
    }
    case 0xa9: {  // RecordAction 8: a punch (DVElementActorHuman::Translate 0xa9)
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = CMD_PUNCH;
        o.damage = (uint16_t)e->param;
        if (sprite->HasAnimation(CMD_PUNCH_ANIM)) seq.push_back(o);
        break;
    }
    default:
        // the hero's and the NPCs' actions (RecordAction): their animation, when they have one
        if (e->command >= 0x2b && e->command < 0xb0) anim(CMD_SCRIPT_ANIM, e->command);
        break;
    }
    if (DVSequenceManager::trace)
        SBLog("  %s #%d instruct cmd 0x%x: %zu orders (at %.0f,%.0f layer %u)", KindName(), DVScript::Handle(this), e->command,
              seq.size(), pos.posMap.x, pos.posMap.y, pos.layer);
    if (seq.empty()) {
        e->SetState(SEQ_DONE);
        return;
    }
    orders = seq;
    newOrder = true;
    isMove = false;
    (void)move;
    defaultCommand = IsNPC() ? CMD_WAIT : CMD_NONE;
    scriptElement = e;
    e->state = SEQ_RUNNING;
}

void DVElementActor::ExecuteImmediately(DVSequenceElement* e) {
    switch (e->command) {
    case 0xa6:  // DVElementActorNPC: lock / unlock the brain
        if (ai) ai->ScriptLockAI(false);
        break;
    case 0xa7:
        if (ai) ai->ScriptUnlockAI();
        break;
    case 0xa8:  // a custom event to his script (IActorNPCScript::FilterEvent)
        if (DVScript::mpInstance) DVScript::mpInstance->FilterEvent(this, nullptr, (uint16_t)e->param);
        break;
    case 0xab:  // SayRemark / HeroSpeaking: the voice comes with the sound
        break;
    case 0x4a:  // DVElementActor 0x4a: put him at the point
        Teleport(e->point, sprite->pos.layer, sprite->pos.sector);
        Stop();
        break;
    default:
        break;
    }
    e->SetState(SEQ_DONE);
}

// DVScript::SetActorLocation (0x0834afd0): the place, its layer and walkable area; the
// obstacle he stands on is the area's that holds the point (DVSectorMotionArea::GetProjectionArea)
void DVElementActor::Teleport(const SBGeoPoint2D& p, uint16_t layer, DVSector* area) {
    DVPositionInterface& pos = sprite->pos;
    DVSectorBuilding* was = Building();
    pos.SetPositionMap(p);
    pos.layer = layer;
    if (area) pos.sector = area;
    // into / out of a house (DVScript::CleanFromHisBuildingBeforeTeleport and
    // AddToHisNewBuildingAfterTeleport, which the scripts call around it)
    DVSectorBuilding* now = Building();
    if (was && was != now) was->Leave(this);
    if (now) {
        now->Enter(this);
        pos.SetObstacle(nullptr);
        pos.ComputePositionAll();
        return;
    }
    DVSightObstacle* ob = nullptr;
    if (pos.sector)
        for (DVSightObstacle* o : pos.sector->obstacles)
            if (o && o->polygon.IsInside_p(p)) {
                ob = o;
                break;
            }
    pos.SetObstacle(ob);
    pos.ComputePositionAll();
    sprite->displayOrder = pos.pos3D.y;
}

void DVElementTarget::Instruct(DVSequenceElement* e) {
    DVElement* saved = DVScript::scriptThis;
    DVScript::scriptThis = this;
    if ((e->command == 0x20 || e->command == 0x22) && sprite->HasAnimation((uint32_t)e->param)) {
        animation = (uint32_t)e->param;
        progression = e->command == 0x22 ? 8 : 0;
        sprite->row = (uint16_t)(sprite->animRows[animation] + sprite->pos.direction);
        sprite->entry = 0;
        sprite->ResetSpriteFrame(false);
    }
    DVScript::scriptThis = saved;
    e->SetState(SEQ_DONE);
}
