#include "stdafx.h"
#include "UIHudStatesWnd.h"

#include "../Actor.h"
#include "../ActorCondition.h"
#include "../EntityCondition.h"
#include "../CustomOutfit.h"
#include "../ActorHelmet.h"
#include "../inventory.h"
#include "../RadioactiveZone.h"

#include "UIStatic.h"
#include "UIProgressBar.h"
#include "UIProgressShape.h"
#include "UIXmlInit.h"
#include "UIHelper.h"
#include "ui_arrow.h"
#include "UIInventoryUtilities.h"
#include "CustomDetector.h"
#include "../ai/monsters/basemonster/base_monster.h"
#include "../PDA.h"
#include "WeaponMagazinedWGrenade.h"


extern BOOL		g_enable_hud_compas;


CUIHudStatesWnd::CUIHudStatesWnd()
:m_b_force_update(true),
	m_timer_1sec(0),
	m_last_health(0.0f),
	m_radia_self(0.0f),
	m_radia_hit(0.0f)
{

	for ( int i = 0; i < ALife::infl_max_count; ++i )
	{
		m_zone_cur_power[i] = 0.0f;
//--		m_zone_max_power[i] = 1.0f;
		m_zone_feel_radius[i] = 1.0f;
	}
	m_zone_hit_type[ALife::infl_rad ] = ALife::eHitTypeRadiation;
	m_zone_hit_type[ALife::infl_fire] = ALife::eHitTypeBurn;
	m_zone_hit_type[ALife::infl_acid] = ALife::eHitTypeChemicalBurn;
	m_zone_hit_type[ALife::infl_psi ] = ALife::eHitTypeTelepatic;
	m_zone_hit_type[ALife::infl_electra] = ALife::eHitTypeShock;

	m_zone_feel_radius_max = 0.0f;
	
	m_health_blink = pSettings->r_float( "actor_condition", "hud_health_blink" );
	clamp( m_health_blink, 0.0f, 1.0f );

	m_fake_indicators_update = false;
//-	Load_section();
}

CUIHudStatesWnd::~CUIHudStatesWnd()
{
}

void CUIHudStatesWnd::reset_ui()
{
	if ( g_pGameLevel )
	{
		Level().hud_zones_list->clear();
	}
}

ALife::EInfluenceType CUIHudStatesWnd::get_indik_type( ALife::EHitType hit_type )
{
	ALife::EInfluenceType iz_type = ALife::infl_max_count;
	switch( hit_type )
	{
	case ALife::eHitTypeRadiation:		iz_type = ALife::infl_rad;		break;
	case ALife::eHitTypeLightBurn:
	case ALife::eHitTypeBurn:			iz_type = ALife::infl_fire;		break;
	case ALife::eHitTypeChemicalBurn:	iz_type = ALife::infl_acid;		break;
	case ALife::eHitTypeTelepatic:		iz_type = ALife::infl_psi;		break;
	case ALife::eHitTypeShock:			iz_type = ALife::infl_electra;	break;// it hasnt CStatic

	case ALife::eHitTypeStrike:
	case ALife::eHitTypeWound:
	case ALife::eHitTypeExplosion:
	case ALife::eHitTypeFireWound:
	case ALife::eHitTypeWound_2:
//	case ALife::eHitTypePhysicStrike:
		return ALife::infl_max_count;
	default:
		NODEFAULT;
	}
	return iz_type;
}

void CUIHudStatesWnd::InitFromXml( CUIXml& xml, LPCSTR path )
{
	CUIXmlInit::InitWindow( xml, path, 0, this );
	XML_NODE* stored_root = xml.GetLocalRoot();
	
	XML_NODE* new_root = xml.NavigateToNode( path, 0 );
	xml.SetLocalRoot( new_root );


	m_back            = UIHelper::CreateStatic( xml, "back", this );





	xml.SetLocalRoot( stored_root );
}

void CUIHudStatesWnd::on_connected()
{
	Load_section();
}

void CUIHudStatesWnd::Load_section()
{
	VERIFY( g_pGameLevel );
	if ( !Level().hud_zones_list )
	{
		Level().create_hud_zones_list();
		VERIFY( Level().hud_zones_list );
	}
	
//	m_actor_radia_factor = pSettings->r_float( "radiation_zone_detector", "actor_radia_factor" );
	Level().hud_zones_list->load( "all_zone_detector", "zone" );

	Load_section_type( ALife::infl_rad,     "radiation_zone_detector" );
	Load_section_type( ALife::infl_fire,    "fire_zone_detector" );
	Load_section_type( ALife::infl_acid,    "acid_zone_detector" );
	Load_section_type( ALife::infl_psi,     "psi_zone_detector" );
	Load_section_type( ALife::infl_electra, "electra_zone_detector" );	//no uistatic
}

void CUIHudStatesWnd::Load_section_type( ALife::EInfluenceType type, LPCSTR section )
{

	m_zone_feel_radius[type] = pSettings->r_float( section, "zone_radius" );
	if ( m_zone_feel_radius[type] <= 0.0f )
	{
		m_zone_feel_radius[type] = 1.0f;
	}
	if ( m_zone_feel_radius_max < m_zone_feel_radius[type] )
	{
		m_zone_feel_radius_max = m_zone_feel_radius[type];
	}
	m_zone_threshold[type] = pSettings->r_float( section, "threshold" );
}

void CUIHudStatesWnd::Update()
{
	CActor* actor = smart_cast<CActor*>( Level().CurrentViewEntity() );
	if ( !actor )
		return;

	if (!g_enable_hud_compas)
	{
		if (m_back->IsShown())
			m_back->Show(false);
	}
	else
	{
		if (!m_back->IsShown())
			m_back->Show(true);
	}

	UpdateActiveItemInfo( actor );
	UpdateIndicators( actor );
	
	UpdateZones();

	inherited::Update();
}



void CUIHudStatesWnd::UpdateActiveItemInfo( CActor* actor )
{
	PIItem item = actor->inventory().ActiveItem();
	if ( item )
	{
		if(m_b_force_update)
		{
			if(item->cast_weapon())
				item->cast_weapon()->ForceUpdateAmmo();
			m_b_force_update		= false;
		}

		item->GetBriefInfo			( m_item_info );
	}
}


// ------------------------------------------------------------------------------------------------
void CUIHudStatesWnd::UpdateZones()
{

	CActor* actor = smart_cast<CActor*>( Level().CurrentViewEntity() );
	if ( !actor )
	{
		return;
	}
	CPda* const pda	= actor->GetPDA();
	if(pda)
	{
		typedef xr_vector<CObject*>	monsters;
		for(monsters::const_iterator it	= pda->feel_touch.begin();
									 it != pda->feel_touch.end(); ++it)
		{
			CBaseMonster* const	monster	= smart_cast<CBaseMonster*>(*it);
			if(!monster || !monster->g_Alive()) 
				continue;

			monster->play_detector_sound();
		}
	}


	m_radia_self = actor->conditions().GetRadiation();
	
	float zone_max_power = actor->conditions().GetZoneMaxPower(ALife::infl_rad);
	float power          = actor->conditions().GetInjuriousMaterialDamage();
	power = power / zone_max_power;
	clamp( power, 0.0f, 1.1f );
	if ( m_zone_cur_power[ALife::infl_rad] < power )
	{
		m_zone_cur_power[ALife::infl_rad] = power;
	}
	m_radia_hit = m_zone_cur_power[ALife::infl_rad];

	if ( !Level().hud_zones_list )
	{
		return;
	}

	for ( int i = 0; i < ALife::infl_max_count; ++i )
	{
		if ( Device.fTimeDelta < 1.0f )
		{
			m_zone_cur_power[i] *= 0.9f * (1.0f - Device.fTimeDelta);
		}
		if ( m_zone_cur_power[i] < 0.01f )
		{
			m_zone_cur_power[i] = 0.0f;
		}
	}

	Fvector posf; 
	posf.set( Device.vCameraPosition );
	Level().hud_zones_list->feel_touch_update( posf, m_zone_feel_radius_max );
	
	if ( Level().hud_zones_list->m_ItemInfos.size() == 0 )
	{
		return;
	}

	CZoneList::ItemsMapIt itb	= Level().hud_zones_list->m_ItemInfos.begin();
	CZoneList::ItemsMapIt ite	= Level().hud_zones_list->m_ItemInfos.end();
	for ( ; itb != ite; ++itb ) 
	{
		CCustomZone*		pZone = itb->first;
		ITEM_INFO&			zone_info = itb->second;
		ITEM_TYPE*			zone_type = zone_info.curr_ref;
		
		ALife::EHitType			hit_type = pZone->GetHitType();
		ALife::EInfluenceType	z_type = get_indik_type( hit_type );
/*		if ( z_type == indik_type_max )
		{
			continue;
		}
*/

		Fvector P			= Device.vCameraPosition;
		P.y					-= 0.5f;
		float dist_to_zone	= 0.0f;
		float rad_zone		= 0.0f;
		pZone->CalcDistanceTo( P, dist_to_zone, rad_zone );
		clamp( dist_to_zone, 0.0f, flt_max * 0.5f );
		
		float fRelPow = ( dist_to_zone / (rad_zone + (z_type==ALife::infl_max_count)? 5.0f : m_zone_feel_radius[z_type] + 0.1f) ) - 0.1f;

		zone_max_power = actor->conditions().GetZoneMaxPower(z_type);
		power = pZone->Power( dist_to_zone, rad_zone );
		//power = power / zone_max_power;
		clamp( power, 0.0f, 1.1f );

		if ( (z_type!=ALife::infl_max_count) && (m_zone_cur_power[z_type] < power) ) //max
		{
			m_zone_cur_power[z_type] = power;
		}

		if ( dist_to_zone < rad_zone + 0.9f * ((z_type==ALife::infl_max_count)?5.0f:m_zone_feel_radius[z_type]) )
		{
			fRelPow *= 0.6f;
			if ( dist_to_zone < rad_zone )
			{
				fRelPow *= 0.3f;
				fRelPow *= ( 2.5f - 2.0f * power ); // звук зависит от силы зоны
			}
		}
		clamp( fRelPow, 0.0f, 1.0f );

		//определить текущую частоту срабатывания сигнала
		zone_info.cur_period = zone_type->freq.x + (zone_type->freq.y - zone_type->freq.x) * (fRelPow * fRelPow);
		
		//string256	buff_z;
		//xr_sprintf( buff_z, "zone %2.2f\n", zone_info.cur_period );
		//xr_strcat( buff, buff_z );
		if( zone_info.snd_time > zone_info.cur_period )
		{
			zone_info.snd_time = 0.0f;
		} 
		else
		{
			zone_info.snd_time += Device.fTimeDelta;
		}
	} // for itb
}

void CUIHudStatesWnd::UpdateIndicators( CActor* actor )
{
	if(m_fake_indicators_update)
		return;

	for ( int i = 0; i < it_max ; ++i ) // it_max = ALife::infl_max_count-1
	{
		UpdateIndicatorType( actor, (ALife::EInfluenceType)i );
	}
}

void CUIHudStatesWnd::UpdateIndicatorType( CActor* actor, ALife::EInfluenceType type )
{
	if ( type < ALife::infl_rad || ALife::infl_psi < type )
	{
		VERIFY2( 0, "Failed EIndicatorType for CStatic!" );
		return;
	}

	LPCSTR texture = "";
	string128 str;
	switch(type)
	{
		case ALife::infl_rad: texture = "ui_inGame2_triangle_Radiation_"; break;
		case ALife::infl_fire: texture = "ui_inGame2_triangle_Fire_"; break;
		case ALife::infl_acid: texture = "ui_inGame2_triangle_Biological_"; break;
		case ALife::infl_psi: texture = "ui_inGame2_triangle_Psy_"; break;
		default: NODEFAULT;
	}
	float           hit_power = m_zone_cur_power[type];
	ALife::EHitType hit_type  = m_zone_hit_type[type];
	
	CCustomOutfit* outfit = actor->GetOutfit();
	CHelmet* helmet = smart_cast<CHelmet*>(actor->inventory().ItemFromSlot(HELMET_SLOT));
	float protect = (outfit) ? outfit->GetDefHitTypeProtection( hit_type ) : 0.0f;
	protect += (helmet) ? helmet->GetDefHitTypeProtection(hit_type) : 0.0f;
	protect += actor->GetProtection_ArtefactsOnBelt( hit_type );

	CEntityCondition::BOOSTER_MAP cur_booster_influences = actor->conditions().GetCurBoosterInfluences();
	CEntityCondition::BOOSTER_MAP::const_iterator it;
	if(hit_type==ALife::eHitTypeChemicalBurn)
	{
		it = cur_booster_influences.find(eBoostChemicalBurnProtection);
		if(it!=cur_booster_influences.end())
			protect += it->second.fBoostValue;
	}
	else if(hit_type==ALife::eHitTypeRadiation)
	{
		it = cur_booster_influences.find(eBoostRadiationProtection);
		if(it!=cur_booster_influences.end())
			protect += it->second.fBoostValue;
	}
	else if(hit_type==ALife::eHitTypeTelepatic)
	{
		it = cur_booster_influences.find(eBoostTelepaticProtection);
		if(it!=cur_booster_influences.end())
			protect += it->second.fBoostValue;
	}


	if ( hit_power < EPS )
	{

		actor->conditions().SetZoneDanger( 0.0f, type );
		return;
	}

	if ( hit_power <= protect )
	{


		actor->conditions().SetZoneDanger( 0.0f, type );
		return;
	}
	if ( hit_power - protect < m_zone_threshold[type] )
	{

		actor->conditions().SetZoneDanger( 0.0f, type );
		return;
	}
	VERIFY(actor->conditions().GetZoneMaxPower(hit_type));
	actor->conditions().SetZoneDanger((hit_power-protect)/actor->conditions().GetZoneMaxPower(hit_type), type);
}

float CUIHudStatesWnd::get_zone_cur_power( ALife::EHitType hit_type )
{
	ALife::EInfluenceType iz_type = get_indik_type( hit_type );
	if ( iz_type == ALife::infl_max_count )
	{
		return 0.0f;
	}
	return m_zone_cur_power[iz_type];
}

void CUIHudStatesWnd::DrawZoneIndicators()
{
	CActor* actor = smart_cast<CActor*>(Level().CurrentViewEntity());
	if(!actor)
		return;

	UpdateIndicators(actor);

}

void CUIHudStatesWnd::FakeUpdateIndicatorType(u8 t, float power)
{
	ALife::EInfluenceType type = (ALife::EInfluenceType)t;
	if ( type < ALife::infl_rad || ALife::infl_psi < type )
	{
		VERIFY2( 0, "Failed EIndicatorType for CStatic!" );
		return;
	}

	CActor* actor = smart_cast<CActor*>( Level().CurrentViewEntity() );
	if(!actor)
		return;

	LPCSTR texture = "";
	string128 str;
	switch(type)
	{
		case ALife::infl_rad: texture = "ui_inGame2_triangle_Radiation_"; break;
		case ALife::infl_fire: texture = "ui_inGame2_triangle_Fire_"; break;
		case ALife::infl_acid: texture = "ui_inGame2_triangle_Biological_"; break;
		case ALife::infl_psi: texture = "ui_inGame2_triangle_Psy_"; break;
		default: NODEFAULT;
	}
	float           hit_power = power;
	ALife::EHitType hit_type  = m_zone_hit_type[type];
	
	CCustomOutfit* outfit = actor->GetOutfit();
	CHelmet* helmet = smart_cast<CHelmet*>(actor->inventory().ItemFromSlot(HELMET_SLOT));
	float protect = (outfit) ? outfit->GetDefHitTypeProtection( hit_type ) : 0.0f;
	protect += (helmet) ? helmet->GetDefHitTypeProtection(hit_type) : 0.0f;
	protect += actor->GetProtection_ArtefactsOnBelt( hit_type );

	CEntityCondition::BOOSTER_MAP cur_booster_influences = actor->conditions().GetCurBoosterInfluences();
	CEntityCondition::BOOSTER_MAP::const_iterator it;
	if(hit_type==ALife::eHitTypeChemicalBurn)
	{
		it = cur_booster_influences.find(eBoostChemicalBurnProtection);
		if(it!=cur_booster_influences.end())
			protect += it->second.fBoostValue;
	}
	else if(hit_type==ALife::eHitTypeRadiation)
	{
		it = cur_booster_influences.find(eBoostRadiationProtection);
		if(it!=cur_booster_influences.end())
			protect += it->second.fBoostValue;
	}
	else if(hit_type==ALife::eHitTypeTelepatic)
	{
		it = cur_booster_influences.find(eBoostTelepaticProtection);
		if(it!=cur_booster_influences.end())
			protect += it->second.fBoostValue;
	}

	float max_power = actor->conditions().GetZoneMaxPower( hit_type );
	protect = protect / max_power; // = 0..1

	if ( hit_power < EPS )
	{
		actor->conditions().SetZoneDanger( 0.0f, type );
		return;
	}

	if ( hit_power < protect )
	{
		actor->conditions().SetZoneDanger( 0.0f, type );
		return;
	}
	if ( hit_power - protect < m_zone_threshold[type] )
	{
		actor->conditions().SetZoneDanger( 0.0f, type );
		return;
	}

	actor->conditions().SetZoneDanger( hit_power - protect, type );
}

void CUIHudStatesWnd::EnableFakeIndicators(bool enable)
{
	m_fake_indicators_update = enable;
}