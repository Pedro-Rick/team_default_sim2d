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

#include "bhv_basic_move.h"

#include "strategy.h"

#include "bhv_basic_tackle.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>

#include "neck_offensive_intercept_neck.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicMove" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 80.0 ).execute( agent ) )
    {
        return true;
    }

    const WorldModel & wm = agent->world();
    /*--------------------------------------------------------*/
    // chase ball
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_pos = wm.ball().pos();
    const bool central_goal_danger
        = ( ball_pos.x < SP.ourPenaltyAreaLineX() + 15.0
            && ball_pos.absY() < SP.penaltyAreaHalfWidth() + 4.0
            && ( wm.existKickableOpponent()
                 || opp_min <= self_min + 3 ) );

    if ( central_goal_danger )
    {
        const int unum = wm.self().unum();
        Vector2D cover_point = Vector2D::INVALIDATED;

        if ( unum == 2 )
        {
            // Left center back protects the central-left shooting lane.
            cover_point.assign( -SP.pitchHalfLength() + 11.0, -4.8 );
        }
        else if ( unum == 3 )
        {
            // Right center back protects the central-right shooting lane.
            cover_point.assign( -SP.pitchHalfLength() + 11.0, 4.8 );
        }
        else if ( unum == 6 )
        {
            // Defensive half sits in front of the box to block the square pass.
            cover_point.assign( SP.ourPenaltyAreaLineX() + 3.0,
                                bound( -8.0, ball_pos.y * 0.45, 8.0 ) );
        }
        else if ( unum == 4 && ball_pos.y < 4.0 )
        {
            cover_point.assign( -SP.pitchHalfLength() + 14.0, -12.0 );
        }
        else if ( unum == 5 && ball_pos.y > -4.0 )
        {
            cover_point.assign( -SP.pitchHalfLength() + 14.0, 12.0 );
        }
        else if ( ( unum == 7 || unum == 8 )
                  && ball_pos.x > SP.ourPenaltyAreaLineX() - 2.0
                  && ball_pos.x < SP.ourPenaltyAreaLineX() + 16.0 )
        {
            // One attacking half drops to deny the free receiver at the arc.
            const double side = ( unum == 7 ? -1.0 : 1.0 );
            cover_point.assign( SP.ourPenaltyAreaLineX() + 9.0,
                                side * 8.5 );
        }

        if ( cover_point.isValid() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": boss central box cover (%d) target=(%.1f %.1f)",
                          unum, cover_point.x, cover_point.y );
            agent->debugClient().addMessage( "BossCenterCover" );
            agent->debugClient().setTarget( cover_point );
            agent->debugClient().addCircle( cover_point, 0.8 );

            if ( ! Body_GoToPoint( cover_point,
                                   0.8,
                                   SP.maxDashPower() ).execute( agent ) )
            {
                Body_TurnToBall().execute( agent );
            }
            agent->setNeckAction( new Neck_TurnToBall() );

            return true;
        }
    }

    const bool deep_side_danger
        = ( ball_pos.x < SP.ourPenaltyAreaLineX() + 8.0
            && ball_pos.absY() > SP.goalHalfWidth() + 1.0
            && ball_pos.absY() < SP.penaltyAreaHalfWidth() + 6.0
            && ( wm.existKickableOpponent()
                 || opp_min <= self_min + 2 ) );

    if ( deep_side_danger )
    {
        const double side = ( ball_pos.y >= 0.0 ? 1.0 : -1.0 );
        const int unum = wm.self().unum();
        Vector2D cover_point = Vector2D::INVALIDATED;

        if ( ( side > 0.0 && unum == 5 )
             || ( side < 0.0 && unum == 4 ) )
        {
            // Same-side side back seals the byline/cutback lane.
            cover_point.assign( -SP.pitchHalfLength() + 7.5,
                                side * ( SP.goalHalfWidth() + 4.0 ) );
        }
        else if ( ( side > 0.0 && unum == 3 )
                  || ( side < 0.0 && unum == 2 ) )
        {
            // Same-side center back protects the near post channel.
            cover_point.assign( -SP.pitchHalfLength() + 4.5,
                                side * ( SP.goalHalfWidth() + 0.8 ) );
        }
        else if ( unum == 6 )
        {
            // Defensive half blocks the pass back to the penalty spot.
            cover_point.assign( SP.ourPenaltyAreaLineX() + 3.0,
                                side * ( SP.goalHalfWidth() + 6.5 ) );
        }

        if ( cover_point.isValid() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": boss deep side cover (%d) target=(%.1f %.1f)",
                          unum, cover_point.x, cover_point.y );
            agent->debugClient().addMessage( "BossSideCover" );
            agent->debugClient().setTarget( cover_point );
            agent->debugClient().addCircle( cover_point, 0.7 );

            if ( ! Body_GoToPoint( cover_point,
                                   0.7,
                                   SP.maxDashPower() ).execute( agent ) )
            {
                Body_TurnToBall().execute( agent );
            }
            agent->setNeckAction( new Neck_TurnToBall() );

            return true;
        }
    }

    if ( wm.existKickableOpponent()
         && wm.ball().pos().x > -35.0
         && wm.ball().distFromSelf() < 13.0
         && self_min <= opp_min + 3 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": benchmark counter press" );
        agent->debugClient().addMessage( "BossPress" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );

        return true;
    }

    if ( ! wm.existKickableTeammate()
         && ( self_min <= 3
              || ( self_min <= mate_min
                   && self_min < opp_min + 4 )
              )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );

        return true;
    }

    const Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicMove target=(%.1f %.1f) dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dist_thr );

    agent->debugClient().addMessage( "BasicMove%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power
                           ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    if ( wm.existKickableOpponent()
         && wm.ball().distFromSelf() < 18.0 )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
    }

    return true;
}
