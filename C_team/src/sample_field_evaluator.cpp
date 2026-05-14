// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sample_field_evaluator.h"

#include "field_analyzer.h"
#include "simple_pass_checker.h"

#include <rcsc/player/player_evaluator.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cfloat>

// #define DEBUG_PRINT

using namespace rcsc;

static const int VALID_PLAYER_THRESHOLD = 8;


/*-------------------------------------------------------------------*/
/*!

 */
static double evaluate_state( const PredictState & state );


/*-------------------------------------------------------------------*/
/*!

 */
SampleFieldEvaluator::SampleFieldEvaluator()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
SampleFieldEvaluator::~SampleFieldEvaluator()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
double
SampleFieldEvaluator::operator()( const PredictState & state,
                                  const std::vector< ActionStatePair > & /*path*/ ) const
{
    const double final_state_evaluation = evaluate_state( state );

    double result = final_state_evaluation;

    return result;
}


/*-------------------------------------------------------------------*/
/*!

 */
static
double
evaluate_state( const PredictState & state )
{
    const ServerParam & SP = ServerParam::i();

    const AbstractPlayerObject * holder = state.ballHolder();

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "========= (evaluate_state) ==========" );
#endif

    //
    // if holder is invalid, return bad evaluation
    //
    if ( ! holder )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX null holder" );
#endif
        return - DBL_MAX / 2.0;
    }

    const int holder_unum = holder->unum();


    //
    // ball is in opponent goal
    //
    if ( state.ball().pos().x > + ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() + 2.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) *** in opponent goal" );
#endif
        return +1.0e+7;
    }

    //
    // ball is in our goal
    //
    if ( state.ball().pos().x < - ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX in our goal" );
#endif

        return -1.0e+7;
    }


    //
    // out of pitch
    //
    if ( state.ball().pos().absX() > SP.pitchHalfLength()
         || state.ball().pos().absY() > SP.pitchHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX out of pitch" );
#endif

        return - DBL_MAX / 2.0;
    }


    const Vector2D ball_pos = state.ball().pos();
    const Vector2D their_goal = SP.theirTeamGoalPos();
    const double goal_dist = their_goal.dist( ball_pos );
    const double center_lane = std::max( 0.0, 18.0 - ball_pos.absY() );

    //
    // set aggressive benchmark evaluation
    //
    double point = 0.0;

    point += ball_pos.x * 2.8;
    point += std::max( 0.0, 52.5 - goal_dist ) * 4.0;
    point += center_lane * 1.7;

    if ( ball_pos.x > 30.0 )
    {
        point += ( ball_pos.x - 30.0 ) * 3.5;
        point += std::max( 0.0, 20.0 - ball_pos.absY() ) * 3.0;
    }
    else if ( ball_pos.x < -25.0 )
    {
        point -= ( -25.0 - ball_pos.x ) * 3.0;
    }

    if ( ball_pos.absY() > SP.pitchHalfWidth() - 6.0 )
    {
        point -= ( ball_pos.absY() - ( SP.pitchHalfWidth() - 6.0 ) ) * 8.0;
    }

    double nearest_opp_dist = 1000.0;
    state.getOpponentNearestTo( ball_pos, VALID_PLAYER_THRESHOLD, &nearest_opp_dist );
    if ( nearest_opp_dist < 2.0 )
    {
        point -= 240.0;
    }
    else if ( nearest_opp_dist < 4.0 )
    {
        point -= ( 4.0 - nearest_opp_dist ) * 45.0;
    }
    else if ( nearest_opp_dist > 8.0 )
    {
        point += std::min( 80.0, ( nearest_opp_dist - 8.0 ) * 12.0 );
    }

    if ( holder_unum >= 9 )
    {
        point += 35.0;
    }
    else if ( holder_unum <= 5 && ball_pos.x > 20.0 )
    {
        point -= 20.0;
    }

    point -= static_cast< double >( state.spendTime() ) * 1.5;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "(eval) ball pos (%f, %f)",
                  ball_pos.x, ball_pos.y );

    dlog.addText( Logger::ACTION_CHAIN,
                  "(eval) initial value (%f)", point );
#endif

    //
    // add bonus for goal, free situation near offside line
    //
    if ( FieldAnalyzer::can_shoot_from
         ( holder->unum() == state.self().unum(),
           holder->pos(),
           state.getPlayerCont( new OpponentOrUnknownPlayerPredicate( state.ourSide() ) ),
           VALID_PLAYER_THRESHOLD ) )
    {
        point += 1.0e+6 + std::max( 0.0, 32.0 - goal_dist ) * 1500.0;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) bonus for goal %f (%f)", 1.0e+6, point );
#endif

        if ( holder_unum == state.self().unum() )
        {
            point += 6.0e+5;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "(eval) bonus for goal self %f (%f)", 5.0e+5, point );
#endif
        }
    }

    return point;
}
