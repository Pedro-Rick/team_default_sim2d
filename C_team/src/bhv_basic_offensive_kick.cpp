// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_basic_offensive_kick.h"

#include "body_force_shoot.h"
#include <rcsc/action/body_advance_ball.h>
#include <rcsc/action/body_dribble.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/geom/sector_2d.h>

using namespace rcsc;

namespace {

bool
is_lane_clear( const WorldModel & wm,
               const Vector2D & from,
               const Vector2D & to,
               const double lane_width )
{
    const Segment2D lane( from, to );
    const PlayerPtrCont & opps = wm.opponentsFromSelf();
    const PlayerPtrCont::const_iterator end = opps.end();
    for ( PlayerPtrCont::const_iterator it = opps.begin();
          it != end;
          ++it )
    {
        if ( (*it)->posCount() > 10 )
        {
            continue;
        }

        if ( lane.dist( (*it)->pos() ) < lane_width )
        {
            return false;
        }
    }

    return true;
}

bool
do_fast_cutback_to_shooter( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();
    const Vector2D self_pos = wm.self().pos();

    if ( self_pos.x < 30.0 )
    {
        return false;
    }

    const bool trapped_wide = ( self_pos.x > 33.0
                               && self_pos.absY() > 10.0 );
    const bool crowded_front = ( self_pos.x > 37.0
                                 && wm.getOpponentNearestToSelf( 10 ) );
    if ( ! trapped_wide
         && ! crowded_front )
    {
        return false;
    }

    const PlayerPtrCont & mates = wm.teammatesFromSelf();
    const PlayerObject * best_receiver = static_cast< const PlayerObject * >( 0 );
    Vector2D best_point = Vector2D::INVALIDATED;
    double best_score = -1.0e10;

    const PlayerPtrCont::const_iterator end = mates.end();
    for ( PlayerPtrCont::const_iterator it = mates.begin();
          it != end;
          ++it )
    {
        const PlayerObject * mate = *it;
        if ( ! mate
             || mate->unum() == wm.self().unum()
             || mate->posCount() > 8 )
        {
            continue;
        }

        Vector2D receive_point = mate->pos() + mate->vel();
        if ( receive_point.x < self_pos.x - 16.0
             || receive_point.x < 25.0
             || receive_point.x > SP.pitchHalfLength() - 2.0
             || receive_point.absY() > 24.0
             || receive_point.dist( self_pos ) < 4.0
             || receive_point.dist( self_pos ) > 28.0 )
        {
            continue;
        }

        double opp_dist = 1000.0;
        wm.getOpponentNearestTo( receive_point, 10, &opp_dist );
        if ( opp_dist < 2.4 )
        {
            continue;
        }

        if ( ! is_lane_clear( wm, wm.ball().pos(), receive_point, 1.8 ) )
        {
            continue;
        }

        const bool shot_lane = is_lane_clear( wm,
                                              receive_point,
                                              SP.theirTeamGoalPos(),
                                              2.6 );
        const bool can_shoot = ( shot_lane
                                 && receive_point.x > 32.0
                                 && receive_point.absY() < 22.0
                                 && receive_point.dist( SP.theirTeamGoalPos() ) < 30.0 );
        if ( ! can_shoot )
        {
            continue;
        }

        double score = 0.0;
        score += receive_point.x * 3.0;
        score += std::max( 0.0, 22.0 - receive_point.absY() ) * 4.0;
        score += std::max( 0.0, 35.0 - receive_point.dist( SP.theirTeamGoalPos() ) ) * 8.0;
        score += std::min( 8.0, opp_dist ) * 10.0;
        if ( can_shoot ) score += 300.0;
        if ( shot_lane ) score += 180.0;
        if ( mate->unum() >= 9 ) score += 35.0;
        if ( receive_point.x < self_pos.x ) score += 45.0; // cutback option

        if ( score > best_score )
        {
            best_score = score;
            best_receiver = mate;
            best_point = receive_point;
        }
    }

    if ( ! best_receiver
         || ! best_point.isValid() )
    {
        return false;
    }

    const double pass_dist = wm.ball().pos().dist( best_point );
    const double first_speed
        = std::min( SP.ballSpeedMax(),
                    std::max( 2.4, 2.0 + pass_dist * 0.12 ) );

    dlog.addText( Logger::TEAM,
                  __FILE__": fast cutback pass to %d point=(%.1f %.1f) speed=%.2f",
                  best_receiver->unum(), best_point.x, best_point.y, first_speed );
    agent->debugClient().addMessage( "FastCutback%d", best_receiver->unum() );
    agent->debugClient().setTarget( best_point );

    if ( Body_SmartKick( best_point,
                         first_speed,
                         first_speed * 0.92,
                         2 ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        return true;
    }

    return false;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicOffensiveKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicOffensiveKick" );

    const WorldModel & wm = agent->world();

    const PlayerPtrCont & opps = wm.opponentsFromSelf();
    const PlayerObject * nearest_opp
        = ( opps.empty()
            ? static_cast< PlayerObject * >( 0 )
            : opps.front() );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    const Vector2D nearest_opp_pos = ( nearest_opp
                                       ? nearest_opp->pos()
                                       : Vector2D( -1000.0, 0.0 ) );

    const Vector2D their_goal = ServerParam::i().theirTeamGoalPos();
    if ( wm.self().pos().x > 30.0
         && their_goal.dist( wm.self().pos() ) < 28.0
         && Body_ForceShoot().execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) benchmark force shoot" );
        agent->debugClient().addMessage( "BossForceShoot" );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( do_fast_cutback_to_shooter( agent ) )
    {
        return true;
    }

    Vector2D pass_point;
    if ( Body_Pass::get_best_pass( wm, &pass_point, NULL, NULL ) )
    {
        if ( pass_point.x > wm.self().pos().x - 1.0 )
        {
            bool safety = true;
            const PlayerPtrCont::const_iterator opps_end = opps.end();
            for ( PlayerPtrCont::const_iterator it = opps.begin();
                  it != opps_end;
                  ++it )
            {
                if ( (*it)->pos().dist( pass_point ) < 4.0 )
                {
                    safety = false;
                }
            }

            if ( safety )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (execute) do best pass" );
                agent->debugClient().addMessage( "OffKickPass(1)" );
                Body_Pass().execute( agent );
                agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
                return true;
            }
        }
    }

    if ( nearest_opp_dist < 7.0 )
    {
        if ( Body_Pass().execute( agent ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (execute) do best pass" );
            agent->debugClient().addMessage( "OffKickPass(2)" );
            agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
            return true;
        }
    }

    // dribble to my body dir
    if ( nearest_opp_dist < 5.0
         && nearest_opp_dist > ( ServerParam::i().tackleDist()
                                 + ServerParam::i().defaultPlayerSpeedMax() * 1.5 )
         && wm.self().body().abs() < 70.0 )
    {
        const Vector2D body_dir_drib_target
            = wm.self().pos()
            + Vector2D::polar2vector(5.0, wm.self().body());

        int max_dir_count = 0;
        wm.dirRangeCount( wm.self().body(), 20.0, &max_dir_count, NULL, NULL );

        if ( body_dir_drib_target.x < ServerParam::i().pitchHalfLength() - 1.0
             && body_dir_drib_target.absY() < ServerParam::i().pitchHalfWidth() - 1.0
             && max_dir_count < 3
             )
        {
            // check opponents
            // 10m, +-30 degree
            const Sector2D sector( wm.self().pos(),
                                   0.5, 10.0,
                                   wm.self().body() - 30.0,
                                   wm.self().body() + 30.0 );
            // opponent check with goalie
            if ( ! wm.existOpponentIn( sector, 10, true ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (execute) dribble to my body dir" );
                agent->debugClient().addMessage( "OffKickDrib(1)" );
                Body_Dribble( body_dir_drib_target,
                              1.0,
                              ServerParam::i().maxDashPower(),
                              2
                              ).execute( agent );
                agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
                return true;
            }
        }
    }

    Vector2D drib_target( 50.0, wm.self().pos().absY() );
    if ( drib_target.y < 20.0 ) drib_target.y = 20.0;
    if ( drib_target.y > 29.0 ) drib_target.y = 27.0;
    if ( wm.self().pos().y < 0.0 ) drib_target.y *= -1.0;
    const AngleDeg drib_angle = ( drib_target - wm.self().pos() ).th();

    // opponent is behind of me
    if ( nearest_opp_pos.x < wm.self().pos().x + 1.0 )
    {
        // check opponents
        // 15m, +-30 degree
        const Sector2D sector( wm.self().pos(),
                               0.5, 15.0,
                               drib_angle - 30.0,
                               drib_angle + 30.0 );
        // opponent check with goalie
        if ( ! wm.existOpponentIn( sector, 10, true ) )
        {
            const int max_dash_step
                = wm.self().playerType()
                .cyclesToReachDistance( wm.self().pos().dist( drib_target ) );
            if ( wm.self().pos().x > 35.0 )
            {
                drib_target.y *= ( 10.0 / drib_target.absY() );
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": (execute) fast dribble to (%.1f, %.1f) max_step=%d",
                          drib_target.x, drib_target.y,
                          max_dash_step );
            agent->debugClient().addMessage( "OffKickDrib(2)" );
            Body_Dribble( drib_target,
                          1.0,
                          ServerParam::i().maxDashPower(),
                          std::min( 5, max_dash_step )
                          ).execute( agent );
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (execute) slow dribble to (%.1f, %.1f)",
                          drib_target.x, drib_target.y );
            agent->debugClient().addMessage( "OffKickDrib(3)" );
            Body_Dribble( drib_target,
                          1.0,
                          ServerParam::i().maxDashPower(),
                          2
                          ).execute( agent );

        }
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        return true;
    }

    // opp is far from me
    if ( nearest_opp_dist > 5.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": opp far. dribble(%.1f, %.1f)",
                      drib_target.x, drib_target.y );
        agent->debugClient().addMessage( "OffKickDrib(4)" );
        Body_Dribble( drib_target,
                      1.0,
                      ServerParam::i().maxDashPower() * 0.4,
                      1
                      ).execute( agent );
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        return true;
    }

    // opp is near

    // can pass
    if ( Body_Pass().execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) pass",
                      __LINE__ );
        agent->debugClient().addMessage( "OffKickPass(3)" );
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        return true;
    }

    // opp is far from me
    if ( nearest_opp_dist > 3.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) opp far. dribble(%f, %f)",
                      drib_target.x, drib_target.y );
        agent->debugClient().addMessage( "OffKickDrib(5)" );
        Body_Dribble( drib_target,
                      1.0,
                      ServerParam::i().maxDashPower() * 0.2,
                      1
                      ).execute( agent );
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        return true;
    }

    if ( nearest_opp_dist > 2.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": hold" );
        agent->debugClient().addMessage( "OffKickHold" );
        Body_HoldBall().execute( agent );
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        return true;
    }

    {
        dlog.addText( Logger::TEAM,
                      __FILE__": clear" );
        agent->debugClient().addMessage( "OffKickAdvance" );
        Body_AdvanceBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
    }

    return true;

}
