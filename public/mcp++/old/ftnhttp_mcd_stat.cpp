#include "ftnhttp_mcd_stat.h"

using namespace storage::ftnhttp;

void CMCDStat::Init()
{
	Add_Report_Head_Item(ST_RECV);
	Add_Report_Head_Item(ST_REPLY);
	Add_Report_Head_Item(ST_TIMEOUT);
	Add_Report_Head_Item(ST_FAIL);

	Add_Report_Body_Row_Item(ST_UPLOAD_FTN);
	Add_Report_Body_Row_Item(ST_UPLOAD_OFP);
	Add_Report_Body_Row_Item(ST_DOWNLOAD_FTN);
	Add_Report_Body_Row_Item(ST_DOWNLOAD_FTN_DATA);
	Add_Report_Body_Row_Item(ST_DOWNLOAD_OFP);
	Add_Report_Body_Row_Item(ST_DOWNLOAD_OFP_DATA);
	Add_Report_Body_Row_Item(ST_DOWNLOAD_OFF);
	Add_Report_Body_Row_Item(ST_DOWNLOAD_OFF_DATA);
	Add_Report_Body_Row_Item(ST_DELETE);

	Add_Report_Body_Col_Item(ST_COL_CALLS);
	Add_Report_Body_Col_Item(ST_COL_FAIL);
}

void CMCDStat::Print_Other()
{
	double total_in = Get_Stat_Value(ST_FLOW_TOTAL_IN)/1024.0;
	double total_out = Get_Stat_Value(ST_FLOW_TOTAL_OUT)/1024.0;

	double upload_ftn = Get_Stat_Value(ST_FLOW_UPLOAD_FTN)/1024.0;
	double upload_ofp = Get_Stat_Value(ST_FLOW_UPLOAD_OFP)/1024.0;
	double download_ftn = Get_Stat_Value(ST_FLOW_DOWNLOAD_FTN)/1024.0;
	double download_ofp = Get_Stat_Value(ST_FLOW_DOWNLOAD_OFP)/1024.0;
	double download_off = Get_Stat_Value(ST_FLOW_DOWNLOAD_OFF)/1024.0;

	m_StatFile.log_p_no_time(LOG_NORMAL, "--------------------------------------------------------------------------\n");
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8s |%8s |\n",
											"", 
											"Mbps",
											"Mbps/s");
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_TOTAL_IN + ":").c_str(), 
											total_in,
											total_in/60.0);
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_TOTAL_OUT + ":").c_str(), 
											total_out,
											total_out/60.0);
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_UPLOAD_FTN + ":").c_str(), 
											upload_ftn,
											upload_ftn/60.0);
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_UPLOAD_OFP + ":").c_str(), 
											upload_ofp,
											upload_ofp/60.0);
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_DOWNLOAD_FTN + ":").c_str(), 
											download_ftn,
											download_ftn/60.0);
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_DOWNLOAD_OFP + ":").c_str(), 
											download_ofp,
											download_ofp/60.0);
	m_StatFile.log_p_no_time(LOG_NORMAL, "%-18s|%8.2f |%8.2f |\n",
											(ST_FLOW_DOWNLOAD_OFF + ":").c_str(), 
											download_off,
											download_off/60.0);
}