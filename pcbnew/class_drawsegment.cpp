/**
 * @file class_drawsegment.cpp
 * @brief Class and functions to handle a graphic segments.
 */

#include "fctsys.h"
#include "macros.h"
#include "wxstruct.h"
#include "gr_basic.h"
#include "bezier_curves.h"
#include "class_drawpanel.h"
#include "kicad_string.h"
#include "colors_selection.h"
#include "trigo.h"
#include "richio.h"
#include "pcbcommon.h"

#include "pcbnew.h"
#include "protos.h"

#include "class_board.h"
#include "class_module.h"
#include "class_drawsegment.h"


DRAWSEGMENT::DRAWSEGMENT( BOARD_ITEM* aParent, KICAD_T idtype ) :
    BOARD_ITEM( aParent, idtype )
{
    m_Width = m_Flags = m_Type = m_Angle = 0;
    m_Shape = S_SEGMENT;
}


DRAWSEGMENT::~DRAWSEGMENT()
{
}


const DRAWSEGMENT& DRAWSEGMENT::operator = ( const DRAWSEGMENT& rhs )
{
    // skip the linked list stuff, and parent

    m_Type         = rhs.m_Type;
    m_Layer        = rhs.m_Layer;
    m_Width        = rhs.m_Width;
    m_Start        = rhs.m_Start;
    m_End          = rhs.m_End;
    m_Shape        = rhs.m_Shape;
    m_Angle        = rhs.m_Angle;
    m_TimeStamp    = rhs.m_TimeStamp;
    m_BezierC1     = rhs.m_BezierC1;
    m_BezierC2     = rhs.m_BezierC1;
    m_BezierPoints = rhs.m_BezierPoints;

    return *this;
}


void DRAWSEGMENT::Copy( DRAWSEGMENT* source )
{
    if( source == NULL )    // who would do this?
        return;

    *this = *source;    // operator = ()
}

void DRAWSEGMENT::Rotate( const wxPoint& aRotCentre, double aAngle )
{
    RotatePoint( &m_Start, aRotCentre, aAngle );
    RotatePoint( &m_End, aRotCentre, aAngle );
}


void DRAWSEGMENT::Flip( const wxPoint& aCentre )
{
    m_Start.y  = aCentre.y - (m_Start.y - aCentre.y);
    m_End.y  = aCentre.y - (m_End.y - aCentre.y);
    if( m_Shape == S_ARC )
    {
        NEGATE( m_Angle );
    }
    SetLayer( ChangeSideNumLayer( GetLayer() ) );
}


const wxPoint DRAWSEGMENT::GetArcEnd() const
{
    wxPoint endPoint;         // start of arc

    switch( m_Shape )
    {
    case S_ARC:
        // rotate the starting point of the arc, given by m_End, through the
        // angle m_Angle to get the ending point of the arc.
        // m_Start is the arc centre
        endPoint  = m_End;         // m_End = start point of arc
        RotatePoint( &endPoint, m_Start, -m_Angle );
    }

    return endPoint;   // after rotation, the end of the arc.
}


/* use GetArcStart() now
const wxPoint DRAWSEGMENT::GetStart() const
{
    switch( m_Shape )
    {
    case S_ARC:
        return m_End;  // the start of the arc is held in field m_End, center point is in m_Start.

    case S_SEGMENT:
    default:
        return m_Start;
    }
}


const wxPoint DRAWSEGMENT::GetEnd() const
{
    wxPoint endPoint;         // start of arc

    switch( m_Shape )
    {
    case S_ARC:
        // rotate the starting point of the arc, given by m_End, through the
        // angle m_Angle to get the ending point of the arc.
        // m_Start is the arc centre
        endPoint  = m_End;         // m_End = start point of arc
        RotatePoint( &endPoint, m_Start, -m_Angle );
        return endPoint;   // after rotation, the end of the arc.
        break;

    case S_SEGMENT:
    default:
        return m_End;
    }
}
*/


void DRAWSEGMENT::SetAngle( double aAngle )
{
    NORMALIZE_ANGLE_360( aAngle );

    m_Angle = (int) aAngle;
}


MODULE* DRAWSEGMENT::GetParentModule() const
{
    if( m_Parent->Type() != PCB_MODULE_T )
        return NULL;

    return (MODULE*) m_Parent;
}


void DRAWSEGMENT::Draw( EDA_DRAW_PANEL* panel, wxDC* DC, int draw_mode, const wxPoint& aOffset )
{
    int ux0, uy0, dx, dy;
    int l_trace;
    int color, mode;
    int radius;

    BOARD * brd =  GetBoard( );

    if( brd->IsLayerVisible( GetLayer() ) == false )
        return;

    color = brd->GetLayerColor( GetLayer() );

    GRSetDrawMode( DC, draw_mode );
    l_trace = m_Width >> 1;  /* half trace width */

    // Line start point or Circle and Arc center
    ux0 = m_Start.x + aOffset.x;
    uy0 = m_Start.y + aOffset.y;

    // Line end point or circle and arc start point
    dx = m_End.x + aOffset.x;
    dy = m_End.y + aOffset.y;

    mode = DisplayOpt.DisplayDrawItems;

    if( m_Flags & FORCE_SKETCH )
        mode = SKETCH;

    if( l_trace < DC->DeviceToLogicalXRel( MIN_DRAW_WIDTH ) )
        mode = FILAIRE;

    switch( m_Shape )
    {
    case S_CIRCLE:
        radius = (int) hypot( (double) (dx - ux0), (double) (dy - uy0) );

        if( mode == FILAIRE )
        {
            GRCircle( &panel->m_ClipBox, DC, ux0, uy0, radius, color );
        }
        else if( mode == SKETCH )
        {
            GRCircle( &panel->m_ClipBox, DC, ux0, uy0, radius - l_trace, color );
            GRCircle( &panel->m_ClipBox, DC, ux0, uy0, radius + l_trace, color );
        }
        else
        {
            GRCircle( &panel->m_ClipBox, DC, ux0, uy0, radius, m_Width, color );
        }

        break;

    case S_ARC:
        int StAngle, EndAngle;
        radius    = (int) hypot( (double) (dx - ux0), (double) (dy - uy0) );
        StAngle  = (int) ArcTangente( dy - uy0, dx - ux0 );
        EndAngle = StAngle + m_Angle;

        if( !panel->m_PrintIsMirrored )
        {
            if( StAngle > EndAngle )
                EXCHG( StAngle, EndAngle );
        }
        else    // Mirrored mode: arc orientation is reversed
        {
            if( StAngle < EndAngle )
                EXCHG( StAngle, EndAngle );
        }


        if( mode == FILAIRE )
            GRArc( &panel->m_ClipBox, DC, ux0, uy0, StAngle, EndAngle,
                   radius, color );

        else if( mode == SKETCH )
        {
            GRArc( &panel->m_ClipBox, DC, ux0, uy0, StAngle, EndAngle,
                   radius - l_trace, color );
            GRArc( &panel->m_ClipBox, DC, ux0, uy0, StAngle, EndAngle,
                   radius + l_trace, color );
        }
        else
        {
            GRArc( &panel->m_ClipBox, DC, ux0, uy0, StAngle, EndAngle,
                   radius, m_Width, color );
        }
        break;
    case S_CURVE:
            m_BezierPoints = Bezier2Poly(m_Start,m_BezierC1, m_BezierC2, m_End);

            for (unsigned int i=1; i < m_BezierPoints.size(); i++) {
                if( mode == FILAIRE )
                    GRLine( &panel->m_ClipBox, DC,
                            m_BezierPoints[i].x, m_BezierPoints[i].y,
                            m_BezierPoints[i-1].x, m_BezierPoints[i-1].y, 0,
                            color );
                else if( mode == SKETCH )
                {
                    GRCSegm( &panel->m_ClipBox, DC,
                            m_BezierPoints[i].x, m_BezierPoints[i].y,
                            m_BezierPoints[i-1].x, m_BezierPoints[i-1].y,
                             m_Width, color );
                }
                else
                {
                    GRFillCSegm( &panel->m_ClipBox, DC,
                                 m_BezierPoints[i].x, m_BezierPoints[i].y,
                                 m_BezierPoints[i-1].x, m_BezierPoints[i-1].y,
                                 m_Width, color );
                }
            }
         break;
    default:
        if( mode == FILAIRE )
        {
            GRLine( &panel->m_ClipBox, DC, ux0, uy0, dx, dy, 0, color );
        }
        else if( mode == SKETCH )
        {
            GRCSegm( &panel->m_ClipBox, DC, ux0, uy0, dx, dy,
                     m_Width, color );
        }
        else
        {
            GRFillCSegm( &panel->m_ClipBox, DC, ux0, uy0, dx, dy,
                         m_Width, color );
        }

        break;
    }
}


// see pcbstruct.h
void DRAWSEGMENT::DisplayInfo( EDA_DRAW_FRAME* frame )
{
    wxString msg;
    wxString coords;

    BOARD*   board = (BOARD*) m_Parent;
    wxASSERT( board );

    frame->ClearMsgPanel();

    msg = wxT( "DRAWING" );

    frame->AppendMsgPanel( _( "Type" ), msg, DARKCYAN );

    wxString    shape = _( "Shape" );

    switch( m_Shape ) {
        case S_CIRCLE:
            frame->AppendMsgPanel( shape, _( "Circle" ), RED );
            break;

        case S_ARC:
            frame->AppendMsgPanel( shape, _( "Arc" ), RED );

            msg.Printf( wxT( "%.1f" ), (double)m_Angle/10 );
            frame->AppendMsgPanel( _("Angle"), msg, RED );
            break;
        case S_CURVE:
            frame->AppendMsgPanel( shape, _( "Curve" ), RED );
            break;

        default:
            frame->AppendMsgPanel( shape, _( "Segment" ), RED );
    }

    wxString start;
    start << GetStart();

    wxString end;
    end << GetEnd();

    frame->AppendMsgPanel( start, end, DARKGREEN );

    frame->AppendMsgPanel( _( "Layer" ), board->GetLayerName( m_Layer ), DARKBROWN );

    valeur_param( (unsigned) m_Width, msg );
    frame->AppendMsgPanel( _( "Width" ), msg, DARKCYAN );
}


EDA_RECT DRAWSEGMENT::GetBoundingBox() const
{
    EDA_RECT bbox;

    bbox.SetOrigin( m_Start );

    switch( m_Shape )
    {
    case S_SEGMENT:
        bbox.SetEnd( m_End );
        bbox.Inflate( (m_Width / 2) + 1 );
        break;

    case S_CIRCLE:
        bbox.Inflate( GetRadius() + 1 );
        break;

    case S_ARC:
    {
        bbox.Merge( m_End );
        wxPoint end = m_End;
        RotatePoint( &end, m_Start, -m_Angle );
        bbox.Merge( end );
    }
    break;

    case S_POLYGON:
    {
        wxPoint p_end;
        MODULE* module = GetParentModule();

        for( unsigned ii = 0; ii < m_PolyPoints.size(); ii++ )
        {
            wxPoint pt = m_PolyPoints[ii];

            if( module ) // Transform, if we belong to a module
            {
                RotatePoint( &pt, module->GetOrientation() );
                pt += module->m_Pos;
            }

            if( ii == 0 )
                p_end = pt;

            bbox.m_Pos.x = MIN( bbox.m_Pos.x, pt.x );
            bbox.m_Pos.y = MIN( bbox.m_Pos.y, pt.y );
            p_end.x   = MAX( p_end.x, pt.x );
            p_end.y   = MAX( p_end.y, pt.y );
        }

        bbox.SetEnd( p_end );
        bbox.Inflate( 1 );
        break;
    }
    }

    bbox.Inflate( (m_Width+1) / 2 );
    return bbox;
}


bool DRAWSEGMENT::HitTest( const wxPoint& aRefPos )
{
    /* Calculate coordinates to test relative to segment origin. */
    wxPoint relPos = aRefPos - m_Start;

    switch( m_Shape )
    {
    case S_CIRCLE:
    case S_ARC:
    {
        int radius = GetRadius();
        int dist  = (int) hypot( (double) relPos.x, (double) relPos.y );

        if( abs( radius - dist ) <= ( m_Width / 2 ) )
        {
            if( m_Shape == S_CIRCLE )
                return true;

            wxPoint startVec = wxPoint( m_End.x - m_Start.x, m_End.y - m_Start.y );
            wxPoint endVec = m_End - m_Start;
            RotatePoint( &endVec, -m_Angle );

            // Check dot products
            if( (long long)relPos.x*startVec.x + (long long)relPos.y*startVec.y < 0 )
                return false;

            if( (long long)relPos.x*endVec.x + (long long)relPos.y*endVec.y < 0 )
                return false;

            return true;
        }
    }
    break;

    case S_CURVE:
        for( unsigned int i= 1; i < m_BezierPoints.size(); i++)
        {
            if( TestSegmentHit( aRefPos,m_BezierPoints[i-1], m_BezierPoints[i-1], m_Width / 2 ) )
                return true;
        }
        break;

    case S_SEGMENT:
        if( TestSegmentHit( aRefPos, m_Start, m_End, m_Width / 2 ) )
            return true;
        break;

    default:
        wxASSERT( 0 );
        break;
    }
    return false;
}


bool DRAWSEGMENT::HitTest( EDA_RECT& refArea )
{
    switch( m_Shape )
    {
        case S_CIRCLE:
        {
            int radius = GetRadius();
            // Text if area intersects the circle:
            EDA_RECT area = refArea;
            area.Inflate( radius );

            if( area.Contains( m_Start ) )
                return true;
        }
        break;

        case S_ARC:
        case S_SEGMENT:
            if( refArea.Contains( GetStart() ) )
                return true;
            if( refArea.Contains( GetEnd() ) )
                return true;
            break;
    }
    return false;
}


wxString DRAWSEGMENT::GetSelectMenuText() const
{
    wxString text;
    wxString temp;

    text.Printf( _( "Pcb Graphic: %s length: %s on %s" ),
                 GetChars( ShowShape( (STROKE_T) m_Shape ) ),
                 GetChars( valeur_param( GetLength(), temp ) ),
                 GetChars( GetLayerName() ) );

    return text;
}


#if defined(DEBUG)
/**
 * Function Show
 * is used to output the object tree, currently for debugging only.
 * @param nestLevel An aid to prettier tree indenting, and is the level
 *          of nesting of this object within the overall tree.
 * @param os The ostream& to output to.
 */
void DRAWSEGMENT::Show( int nestLevel, std::ostream& os )
{
    NestedSpace( nestLevel, os ) << '<' << GetClass().Lower().mb_str() <<

    " shape=\"" << m_Shape << '"' <<
/*
    " layer=\"" << GetLayer() << '"' <<
    " width=\"" << m_Width << '"' <<
    " angle=\"" << m_Angle << '"' <<  // Used only for Arcs: Arc angle in 1/10 deg
*/
    '>' <<
    "<start" << m_Start << "/>" <<
    "<end"   << m_End << "/>"
    "<GetStart" << GetStart() << "/>" <<
    "<GetEnd"   << GetEnd() << "/>"
    ;

    os << "</" << GetClass().Lower().mb_str() << ">\n";
}
#endif
