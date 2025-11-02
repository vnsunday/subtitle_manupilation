#include "CSubtitleManipulate.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

using namespace std;

#define MARK_TYPE_INDEX 	1
#define MARK_TYPE_TIME 		2
#define MARK_TYPE_CONTENT 	3
#define MARK_TYPE_BLANK 	4

CSubtitleManipulate::~CSubtitleManipulate()
{
}

int CSubtitleManipulate::load_FromFile(const char* sz_File, std::vector<SubtitleLine>& v_out, SuccessResult& ret)
{
    /* A state machine for reading the Subtitle file */
	ifstream file(sz_File);
	string line;
	
	int n_Ret = 0;
	int last_type = 0;
	int loop_Passed_Type_ = 0;
	int arr[1000];
	int candiate_Mark_Types[2];
	int count_Candidates_;

	std::vector<int> v;			// Vector of mark types
	std::vector<string> v_c; 	// Vector of contents
	std::vector<string> v_t;	// Vector of time

	while (std::getline(file, line))
	{
		printf("Load line=%s\r\n", line.c_str());
		next_candidateMarkTypes(last_type, candiate_Mark_Types, count_Candidates_);
		loop_Passed_Type_ = 0;

		for (int i=0;i<count_Candidates_;++i)
		{
			if (is_MarkType(line.c_str(), candiate_Mark_Types[i]))
			{
				loop_Passed_Type_ = candiate_Mark_Types[i];
				break;
			}
		}

		if (loop_Passed_Type_ > 0)
		{
			// OK
			v.push_back(loop_Passed_Type_);

			if (loop_Passed_Type_ == MARK_TYPE_TIME)
			{
				v_t.push_back(line);
			}
			else if (loop_Passed_Type_ == MARK_TYPE_CONTENT)
			{
				if (last_type == MARK_TYPE_CONTENT)
				{
					v_c[v_c.size()-1] = v_c[v_c.size()-1] + " " + line;	// Concat the new string
				}
				else 
				{
					v_c.push_back(line);
				}
			}
			else if (loop_Passed_Type_ == MARK_TYPE_INDEX)
			{
				int index = atoi(line.c_str());
				// TODO: validate the index
			}
			last_type = loop_Passed_Type_;
		}
		else 
		{
			// Invalid file
			// Inform that the current file is invalid.				
			// TODO: Add more information
			n_Ret = 1;	// 
			ret.tag_Data_00 = v_c.size();	// The line which contains error
			ret.tag_Data_01 = loop_Passed_Type_;

			printf("Error Here %s\r\n", line.c_str());
			printf("LastType = %d\r\n", last_type);

			printf("Candidate=");
			for (int i=0;i<count_Candidates_;++i) {
				printf("%d,", candiate_Mark_Types[i]);
			}
			printf("\r\n");
			break;
		}
	}

	// Success
	if (n_Ret == 0)
	{
		v_out.clear();
		for (int i=0;i<v_t.size();i++) {
			SubtitleLine sl;
            string s_From_Time;
            string s_To_Time;
			long fromTimeVal;
			long to_Time_Val;

            parse_Subtitle_Time(v_t[i].c_str(), s_From_Time, s_To_Time, fromTimeVal, to_Time_Val);

			sl.index = i;
			sl.fromTheTime = s_From_Time;			
			sl.to_Time = s_To_Time;
			sl.l_From_Time = fromTimeVal;
			sl.l_ToTheTime = to_Time_Val;
			sl.content = v_c[i];

			v_out.push_back(sl);
		}

		printf("Success(n=%d)\r\n", v_out.size());
	}

	ret.return_Code = n_Ret;	
    return n_Ret;
}

int CSubtitleManipulate::groupingSentences(std::vector<SubtitleLine> v_sub_title, std::vector<SubtitleLine>& v_out)
{
	//@Temp
	printf("Start GroupingSentences(line=%d)\r\n", v_sub_title.size());

	double d_half_flag = 0;	// Flag for the Bulding state of last element 
	v_out.clear();
    
	for (int i=0;i<v_sub_title.size();++i)
	{
		if (d_half_flag == 0)		// Need new elements
		{
			SubtitleLine new_E;
			new_E = v_sub_title[i];

			v_out.push_back(new_E);
			d_half_flag = 0.5;
		}
		else if (d_half_flag == 0.5)	// The last element is building
		{
			int newLen = v_out[v_out.size()-1].content.size() + v_sub_title[i].content.size();
			// Is the end of a sentence?
			//		End sentence = 
			if (i==v_sub_title.size() -1 || v_sub_title[i].content[v_sub_title[i].content.size()-1] == '.' || newLen > 120)
			{
				// Concat content to the last Element & update ToTime 
				v_out[v_out.size()-1].content += " " + v_sub_title[i].content;
				v_out[v_out.size()-1].to_Time = v_sub_title[i].to_Time;
				v_out[v_out.size()-1].l_ToTheTime = v_sub_title[i].l_ToTheTime;

				//	Then mark that the last element finish
				d_half_flag = 0;
			}
			else 
			{
				// Concat content to the last Element & update ToTime
				v_out[v_out.size()-1].content += " " + v_sub_title[i].content;
				v_out[v_out.size()-1].to_Time = v_sub_title[i].to_Time;
				v_out[v_out.size()-1].l_ToTheTime = v_sub_title[i].l_ToTheTime;
			}
		}
	}

	printf("After grouping(vout.size=%d)\r\n", v_out.size());

    return 0;
}

int CSubtitleManipulate::groupingSentences_ByBlockTime(int block_ByMilliSecs,std::vector<SubtitleLine> v_sub_title, std::vector<SubtitleLine>& v_out)
{
	// Use "Virtual Ruler". See SOLUTION.md
	int nRulerWidth = block_ByMilliSecs;
	int anchorPoint = 0;
	int label_BeenProcessed = 0;

	printf("len = %d\r\n", v_sub_title.size());
	v_out.clear();
	if (v_sub_title.size() > 0)
	{
		while (label_BeenProcessed < v_sub_title.size())
		{
			// Put the Ruler to the anchor Point
			SubtitleLine new_E;
			
			int begin_Ruler;
			int endingRuler;
			int i = label_BeenProcessed;

			anchorPoint = v_sub_title[i].l_From_Time;
			begin_Ruler = anchorPoint;
			endingRuler = anchorPoint + nRulerWidth;

			printf("Scanning at(i=%d): begin_Ruler=%d; ending_Ruler=%d; \r\n", i, begin_Ruler, endingRuler);
			printf("\tv_subtitle[%d]={l_fromTime=%d; l_toTheTime=%d; fromTime=%s; ToTime=%s}\r\n", 
								i, v_sub_title[i].l_From_Time,
								v_sub_title[i].l_ToTheTime,
								v_sub_title[i].fromTheTime.c_str(),
								v_sub_title[i].to_Time.c_str());

			new_E.content = "";
			new_E.l_From_Time = v_sub_title[i].l_From_Time;
			new_E.fromTheTime = v_sub_title[i].fromTheTime;
			new_E.l_ToTheTime = -1;

			while (i < v_sub_title.size() && endingRuler > v_sub_title[i].l_From_Time && 
					v_sub_title[i].l_ToTheTime >= begin_Ruler)	// overlapped between [beginRule, endingRuler) and [v_sub_title[i].l_From_Time, v_sub_title[i].l_ToTheTime]
			{
				printf("Find a overlapped: %s\r\n", v_sub_title[i].content.c_str());
				printf("\tTime=(%s --> %s) = (%d --> %d)\r\n", v_sub_title[i].fromTheTime.c_str(), v_sub_title[i].to_Time.c_str(), v_sub_title[i].l_From_Time, v_sub_title[i].l_ToTheTime);
				printf("\tPosition = %d\r\n", i);

				new_E.content += new_E.l_ToTheTime < 0 ? v_sub_title[i].content : string("\r\n") + v_sub_title[i].content;
				new_E.l_ToTheTime = v_sub_title[i].l_ToTheTime;
				new_E.to_Time = v_sub_title[i].to_Time;

				if (new_E.l_ToTheTime < 0)
				{
					new_E.l_ToTheTime = v_sub_title[i].l_ToTheTime;
					new_E.to_Time = v_sub_title[i].to_Time;
				}
				i++;
			}

			if (i > label_BeenProcessed)
			{
				printf("*****************************************ANEW\r\n");
				v_out.push_back(new_E);
			}
			else 
			{
				printf("ERRRORRRRRRRRRRRRRRRRRR at i=%d; LabelProcesssed=%d\r\n", i, label_BeenProcessed);
				break;
			}

			label_BeenProcessed = i;
		}
	}

    return 0;
}

int CSubtitleManipulate::writeToFile(const char* sz_File_Out, std::vector<SubtitleLine> v_sub)
{
	printf("WRite to File\r\n");
	int ret = 0;
	ofstream myfile (sz_File_Out);

  	if (myfile.is_open())
  	{
		for (int i=0;i<v_sub.size();++i)
		{
			myfile << (i+1) << endl;
			myfile << v_sub[i].fromTheTime << " --> " << v_sub[i].to_Time << endl;
			myfile << v_sub[i].content << endl;
			myfile << endl;
		}
		myfile.close();
  	}
  	else 
	{
		// cout << "Unable to open file";
		ret = 1;
	}
    return ret;
}

int CSubtitleManipulate::next_candidateMarkTypes(int currentType, int arr_outCandidates[2], int& n_count)
{
	int ret = 1;
	if (currentType == MARK_TYPE_CONTENT)
	{
		ret = 1;	// Ret = 1 || 3 is OK
		arr_outCandidates[0] = MARK_TYPE_BLANK;
		arr_outCandidates[1] = MARK_TYPE_CONTENT;
		n_count = 2;
	}
	else if (currentType == MARK_TYPE_INDEX || currentType == MARK_TYPE_TIME)
	{
		arr_outCandidates[0] = currentType + 1;
		n_count = 1;
	}
	else if (currentType == MARK_TYPE_BLANK)
	{
		arr_outCandidates[0] = MARK_TYPE_INDEX;
		n_count = 1;
	}
	else 
	{
		// Default case
		arr_outCandidates[0] = MARK_TYPE_INDEX;
		n_count = 1;
	}

	return ret;
}

bool CSubtitleManipulate::is_MarkType(const char* szLine, int n_mark_Type)
{
    // 00:02:17,440 --> 00:02:20,375
	std::regex regex_integer("[0-9]+"); // ("[[:digit:]]+");
	std::regex regex_subtime("([0-9]{2}\\:[0-9]{2}\\:[0-9]{2}\\,[0-9]{3})[[:space:]]*-->[[:space:]]*([0-9]{2}\\:[0-9]{2}\\:[0-9]{2}\\,[0-9]{3})");

    if (n_mark_Type == MARK_TYPE_INDEX)
    {
        return std::regex_match(szLine, regex_integer);
    }
    else if (n_mark_Type == MARK_TYPE_TIME)
    {
        return std::regex_match(szLine, regex_subtime);
    }
    else if (n_mark_Type == MARK_TYPE_CONTENT)
    {
        return true;    // Always true
    }
    else if (n_mark_Type == MARK_TYPE_BLANK)
    {
        std::string s(szLine);
        return s.empty();
    }
    else 
    {
        return false;
    }
}

int CSubtitleManipulate::parse_Subtitle_Time(const char* szLine, std::string& str_from_Time, std::string& str_To_Time, long& fromTimeVal, long& to_Time_Val)
{
    int ret = 1;
    std::regex regex_subtime("([0-9]{2}\\:[0-9]{2}\\:[0-9]{2}\\,[0-9]{3})[[:space:]]*-->[[:space:]]*([0-9]{2}\\:[0-9]{2}\\:[0-9]{2}\\,[0-9]{3})");
	std::regex regex_milli("([0-9]{2})\\:([0-9]{2})\\:([0-9]{2})\\,([0-9]{3})");
    std::cmatch subtime_match;
	std::cmatch milli_match;

	long fromTimeMilli = 0;
	long toTimeMilli = 0;

    if (regex_match(szLine, subtime_match, regex_subtime) && subtime_match.size() == 3)
    {
        str_from_Time = subtime_match[1];
        str_To_Time = subtime_match[2];
        ret = 0;    // Success
    
		regex_match(str_from_Time.c_str(), milli_match, regex_milli);
		int n_startTime = (atoi(string(milli_match[1]).c_str()) * 60 * 60  + 
							atoi(string(milli_match[2]).c_str()) * 60 + 
							atoi(string(milli_match[3]).c_str()) )  * 1000 + 
							atoi(string(milli_match[4]).c_str());

		regex_match(str_To_Time.c_str(), milli_match, regex_milli);
		int n_ToTheTime = (atoi(string(milli_match[1]).c_str()) * 60 * 60  + 
							atoi(string(milli_match[2]).c_str()) * 60 + 
							atoi(string(milli_match[3]).c_str()) )  * 1000 + 
							atoi(string(milli_match[4]).c_str());

		fromTimeVal = n_startTime;
		to_Time_Val = n_ToTheTime;

		// @@Data Error
		// Work-around with Time error
		if (to_Time_Val < fromTimeVal)
		{
			to_Time_Val = fromTimeVal;
			str_To_Time = str_from_Time;
		}
	}

    return ret;
}