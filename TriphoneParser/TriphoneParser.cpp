// TriphoneParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <fstream>
#include <map>
#include <algorithm>
#include <vector>
#include <list>
#include <utility>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip> 
#include <Windows.h>
#include <set>
#include <tuple>
#include <functional>

using namespace std;

//-------------------------------------------------
// ТИПЫ ДАННЫХ:
//-------------------------------------------------
typedef list <string>					PhonemeList;
typedef list <PhonemeList>				TranscriptionsList;

struct  DictionaryItem
{
	DictionaryItem() :	added_to_triphonelist(false){}
	TranscriptionsList	transcriptions;
	bool				added_to_triphonelist;
};

typedef tuple <string, string, string, char>	Triphone;

struct ModelDefinition
{
	struct Line
	{
		Triphone	triphone;
		string		attrib;
		int			tmat_n;
		int			base_sen_n;
		int			left_sen_n;
		int			right_sen_n;
		string		something_n;
	};
	typedef vector <Line>	Lines;

	ModelDefinition() {n_ci = n_tri = n_map = n_ci_sen = n_sen = n_tmat = 0;}

	string	version;
	int		n_ci, n_tri, n_map, n_ci_sen, n_sen, n_tmat;
	Lines	lines;
};

typedef function<bool(const vector <string>&)>	LineFunction;
typedef map <string, DictionaryItem>			Dictioany;
typedef map <Triphone, int>						TriphoneSet;
typedef vector <int>							SenoneMap;

//-------------------------------------------------
// ПОРТОТИПЫ ФУНКЦИЙ
//-------------------------------------------------
bool	load_dictionary (const string&, Dictioany&);
bool	find_triphones (const string&, Dictioany&, TriphoneSet&);
bool	read_mdef (const string&, ModelDefinition &, TriphoneSet &);
bool	remap_senones(ModelDefinition&, const SenoneMap&);
bool	inverse_senone_map(const SenoneMap&, SenoneMap&);
bool	generate_copy(const string&, const string&, const SenoneMap&, const vector <INT32>&, int, int);
bool	add_triphones_from_transcription(const PhonemeList &trans, TriphoneSet& triphones);
void	add_triphones_from_sentence(const vector <PhonemeList *>& , TriphoneSet& );
bool	add_triphones_from_sentence (Dictioany&, const list <string>&, TriphoneSet&);
bool	print_triphone_freq(const TriphoneSet& );

int		build_senone_map(const ModelDefinition&, SenoneMap&);
void	generate_mdef (const ModelDefinition& , const SenoneMap& );
void	parse_cmd_line (int argc, _TCHAR* argv[]);
void	print_params();
string	utf8_to_ansi (const string& );
ostream& operator <<(ostream&, const Triphone&);
ostream& operator <<(ostream&, const TriphoneSet&);
ostream& operator <<(ostream&, const ModelDefinition::Lines&);
ostream& operator <<(ostream&, const SenoneMap& );

#define NUMEL(x) (sizeof(x) / sizeof(*(x)))
//-------------------------------------------------
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
//-------------------------------------------------
string			dict_path		= "C:\\Users\\Andrey\\Documents\\Visual Studio 2012\\Projects\\Sphinx\\pocketsphinx\\voxforge-ru-0.2\\etc\\andky_words.dic";
string			acmod_path		= "C:\\Users\\Andrey\\Documents\\Visual Studio 2012\\Projects\\Sphinx\\pocketsphinx\\voxforge-ru-0.2\\model_parameters\\msu_ru_nsh_breath.cd_cont_1000_8gau_8000";
string			sentences_path	= "C:\\Users\\Andrey\\Documents\\Visual Studio 2012\\Projects\\Sphinx\\pocketsphinx\\voxforge-ru-0.2\\etc\\sentenses.txt";
string			work_dir		= "model_params";

const string	sil("SIL");
const char*		mdef_attr[]			= {"n_base", "n_tri", "n_state_map", "n_tied_state", "n_tied_ci_state", "n_tied_tmat"};
const char*		justcopy_files[]	= {"noisedict", "transition_matrices", "feat.params"};
const char*		args_name[]			= {"-dict", "-hmm", "-s", "-dir"};
string *		args_ptr[]			= {&dict_path, &acmod_path, &sentences_path, &work_dir};
//-------------------------------------------------
// РЕАЛИЗАЦИЯ
//-------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
	Dictioany		dict;
	TriphoneSet		triphones;
	ModelDefinition	mdef;
	SenoneMap		senmap, inv_senmap;

	parse_cmd_line(argc, argv);
	print_params();
    
	setlocale(0, ".1251"); // ”становим кодировку

	if (!load_dictionary(dict_path, dict) ||
		!find_triphones (sentences_path, dict, triphones) ||
		!print_triphone_freq (triphones) ||
		!read_mdef(acmod_path + "\\mdef", mdef, triphones))
	{
		return -1;
	}

	mdef.n_sen = build_senone_map(mdef, senmap);

	if (!remap_senones(mdef, senmap) ||
		!inverse_senone_map(senmap, inv_senmap))
	{
		return -1;
	}

	std::cout << "List of used trifones(" << mdef.lines.size() << "):" << endl << mdef.lines << endl;
	std::cout << "Senon map:\n" << senmap << endl;
	std::cout << "Inverse senon map:\n" << inv_senmap << endl;
	

	CreateDirectory(work_dir.c_str(), NULL);
	SetCurrentDirectory(work_dir.c_str());

	generate_mdef(mdef, senmap);

	INT32			ma[] = {mdef.n_sen, 1, 8, 39, mdef.n_sen * 8 * 39};
	INT32			wa[] = {mdef.n_sen, 1, 8, mdef.n_sen * 8};
	vector <INT32>	vmean_attr (ma, ma + 5);
	vector <INT32>	vweight_attr (wa, wa + 4);

	std::cout << "Generate means...\n";
	if (!generate_copy(acmod_path + "\\means", "means", inv_senmap, vmean_attr, mdef.n_sen, 39 * 8))
		return -1;

	std::cout << "Generate variances...\n";
	if (!generate_copy(acmod_path + "\\variances", "variances", inv_senmap, vmean_attr, mdef.n_sen, 39 * 8))
		return -1;

	std::cout << "Generate mixture_weights...\n";
	if (!generate_copy(acmod_path + "\\mixture_weights", "mixture_weights", inv_senmap, vweight_attr, mdef.n_sen, 8))
		return -1;

	for (int i = 0; i < NUMEL (justcopy_files) ; i++)
	{
		string  path = acmod_path + "\\" + justcopy_files[i];
		CopyFile(path.c_str(), justcopy_files[i], FALSE);
	}

	std::cout << "Sucessful complete!\n";
	std::cout << "Total number of CI triphones: " <<  setw(20) << mdef.n_ci << endl;
	std::cout << "Total number of CD triphones: " <<  setw(20) << mdef.n_tri << endl;
	std::cout << "Total number of senons: "		  <<  setw(20) << mdef.n_sen << endl;

	return 0;
}

bool print_triphone_freq(const TriphoneSet& triphones)
{
	set <tuple<int, const Triphone*> > f;

	for (auto &it : triphones)
	{
		f.insert(make_tuple(it.second, &it.first));
	}

	cout << "Triphone frequencies:\n";
	for (auto &t : f)
	{
		cout << setw(4) << get<0>(t) << *get<1>(t) << endl;
	}
	return true;
}

void parse_cmd_line (int argc, _TCHAR* argv[])
{
	for (int i = 1; i < argc ; i += 2)
	{
		auto it = find_if(args_name, args_name + NUMEL(args_name), [argv, i] (const char* name)
					{
						return !_strcmpi(argv[i], name);
					});
		int idx = it - args_name;
		if (idx < NUMEL(args_name))
		{
			*args_ptr[idx] = argv[i + 1];
		}
		else
		{
			cout << "Unknown cmd line argument " << argv[i] << endl;
		}
	}
}

void print_params()
{
	cout << "Params:\n";
	for (int i = 0; i < NUMEL(args_name); i++)
	{
		cout << left << setw(10) << args_name[i] << left << setw(50) << *args_ptr[i] << endl;
	}
}

bool read_bio_header(ifstream& file, string& bio_header)
{
	bio_header.clear();

	if (file)
	{
		ostringstream	oss;
		string			line, str;
		bool			header_begun = false;
		int				max_line_n = 10;

		while (file.good() && --max_line_n > 0)
		{
			getline (file, line);

			istringstream iss(line);

			if (header_begun)
			{
				iss >> str;
				if (str != "chksum0")
				{
					oss << line << endl;
				}
				if (str == "endhdr")
				{
					bio_header = oss.str();
					return true;
				}
			}
			else
			{
				iss >> str;
				if (str != "s3")
				{
					cout << "Cant find begin bio header\n";
					return false;
				}
				header_begun = true;
				oss << line << endl;
			}
		}
		cout << "Cant find end bio header\n";
	}
	return false;
}

bool skip_magic_number (ifstream& file, UINT32 magic_number)
{
	UINT32 x;
	if (file)
	{
		char byte;
		while (file.good())
		{
			file.get(byte);
			x = (x >> 8) | ((UINT8)byte << 24);

			if (x == magic_number)
				return true;
		}
		cout << "Cant find magic number\n";
	}
	return false;
}

bool inverse_senone_map(const SenoneMap& senmap, SenoneMap& inv_senmap)
{
	auto itmax = max_element(senmap.begin(), senmap.end());
	int maxval = *itmax;
	
	inv_senmap.clear();
	inv_senmap.resize(maxval + 1, -1);
	for (int i = 0; i < senmap.size() ; i++)
	{
		if (senmap[i] >= 0)
			inv_senmap[senmap[i]] = i;
	}

	if (count(inv_senmap.begin(), inv_senmap.end(), -1) > 0)
	{
		cout << "Inverse senon table error\n";
		return false;
	}
	return true;
}

bool generate_copy(const string& infilename, const string& outfilename, const SenoneMap& inv_senmap, 
						const vector <INT32>& mdef_attr, int n_sen, int block_length)
{
	UINT32 magic_number = 0x11223344;

	ifstream file(infilename, ios::in | ios::binary);

	if (file)
	{
		string bio_header;

		if (!read_bio_header(file, bio_header) ||
			!skip_magic_number(file, magic_number))
		{
			return false;
		}

		INT32 val;
		for (int i = 0; i < mdef_attr.size() ; i++)
		{
			file.read((char*)&val, sizeof(INT32));
		}

		// val - хранит количество элементов данных
		vector <UINT32>	data(val);
		file.read ((char*) &data[0], sizeof(UINT32) * val);

		vector <UINT32> outdata(n_sen * block_length);
		for (int i = 0; i < n_sen ; i++)
		{
			copy(data.begin() + inv_senmap[i] * block_length, 
				 data.begin() + (inv_senmap[i] + 1) * block_length, 
				 outdata.begin() + i * block_length);
		}

		ofstream outfile(outfilename, ios::out | ios::binary);

		if (outfile)
		{
			outfile << bio_header;

			outfile.write((char*)&magic_number, sizeof(magic_number));
			outfile.write((char*)&mdef_attr[0], sizeof(INT32) * mdef_attr.size());
			outfile.write((char*)&outdata[0], sizeof(INT32) * outdata.size());

			outfile.flush();
			return true;
		}
		else
		{
			cout << "Cant open file " << outfilename << endl;
		}
	}
	else
	{
		cout << "Cant open file " << infilename << endl;
	}
	return false;
}

bool remap_senones(ModelDefinition& mdef, const SenoneMap& senmap)
{
	for (auto& line : mdef.lines)
	{
		if (line.base_sen_n >= senmap.size() ||
			line.left_sen_n >= senmap.size() ||
			line.right_sen_n >= senmap.size() ||
			senmap[line.base_sen_n] < 0 ||
			senmap[line.left_sen_n] < 0 ||
			senmap[line.right_sen_n] < 0)
		{
			cout << "Remap error for triphone " << line.triphone << endl;
			return false;
		}
		line.base_sen_n = senmap[line.base_sen_n];
		line.left_sen_n = senmap[line.left_sen_n];
		line.right_sen_n = senmap[line.right_sen_n];
	}
	return true;
}

void generate_mdef (const ModelDefinition& mdef, const SenoneMap& senmap)
{
	int attr[] = {mdef.n_ci, mdef.n_tri, mdef.n_map, mdef.n_sen, mdef.n_ci_sen, mdef.n_tmat};

	ofstream file("mdef", ios_base::out);
	if (file)
	{
		file << mdef.version << endl;
		for (int i = 0; i < NUMEL(mdef_attr); i++)
			file << attr[i] << " " << mdef_attr[i] << endl;
	}
	file << "#" << endl;
	file << "# Columns definitions" << endl;
	file << "#base lft  rt p attrib tmat      ... state id's ..." << endl;
	
	file << mdef.lines;
}

int build_senone_map(const ModelDefinition& mdef, SenoneMap& senmap)
{
	senmap.clear();

	auto itmax = max_element(mdef.lines.begin(), mdef.lines.end(), 
		[] (const ModelDefinition::Line& a, const ModelDefinition::Line& b)
		{
			return max(max(a.left_sen_n, a.right_sen_n), a.base_sen_n) <
				   max(max(b.left_sen_n, b.right_sen_n), b.base_sen_n);
		});

	int maxval = max(max(itmax->left_sen_n, itmax->right_sen_n), itmax->base_sen_n); 

	senmap.resize(maxval + 1, -1);

	// Помечаем используемые номера сенонов
	for (const auto& line : mdef.lines)
	{
		senmap[line.base_sen_n] = 0;
		senmap[line.left_sen_n] = 0;
		senmap[line.right_sen_n] = 0;
	}

	int j = 0;
	for (int i = 0; i < senmap.size() ; i++)
	{
		if (senmap[i] == 0)
			senmap[i] = j++;
	}

	return j;
}

bool for_each_mdef_line(ifstream &file, const LineFunction& action)
{

	if (file)
	{
		string			line;
		vector <string>	tokens;

		while (file.good())
		{		
			getline (file, line);

			if (line.empty() || line.front() == '#')	// Комментарий или пустая строка
				continue;

			tokens.clear();

			copy(istream_iterator<string>(istringstream(line)),
					istream_iterator<string>(),
					back_inserter(tokens));

			if (tokens.empty())
				continue;

			if (!action(tokens))
				return false;
		}
		return true;
	}
	return false;
}

bool read_mdef (const string& filename, ModelDefinition &mdef, TriphoneSet &triphones)
{
	map <string, int> attributes;

	ifstream file(filename, ios_base::in);

	if (file)
	{
		int		nline;
		bool	ok;
		
		std::cout << "Reading mdef file\n";

		// Читаем версию mdef
		for_each_mdef_line (file, [&mdef] (const vector <string>& tokens)
		{
			mdef.version = tokens.front();
			return false;
		});

		nline = NUMEL(mdef_attr);

		// Читаем атрибуты
		for_each_mdef_line (file, [&attributes, &nline] (const vector <string>& tokens)
		{
			if (tokens.size() != 2)	return false;
			
			attributes[tokens.back()] = atoi(tokens.front().c_str());

			return --nline > 0;
		});

		// Проверяем что все атрибуты на месте
		for (int i = 0; i < NUMEL(mdef_attr) ; i++)
			if (attributes.count(mdef_attr[i]) == 0)
			{
				cout << "Cant find attribute \"" << mdef_attr[i] <<"\" in mdef file\n";
				return false;
			}

		// Сохраняем
		mdef.n_ci	= attributes["n_base"];
		mdef.n_tri	= attributes["n_tri"];
		mdef.n_map	= attributes["n_state_map"];
		mdef.n_ci_sen = attributes["n_tied_ci_state"];
		mdef.n_sen	= attributes["n_tied_state"];
		mdef.n_tmat = attributes["n_tied_tmat"];

		int total = mdef.n_ci + triphones.size();
		mdef.lines.reserve(total);

		// Читаем контекстно-независимые фонемы 
		ok = for_each_mdef_line (file, [&mdef, &triphones] (const vector <string>& tokens)
		{
			if (tokens.size() != 10) 
				return false;
			
			Triphone	cur_tri(tokens[0], tokens[1], tokens[2], tokens[3][0]); 
			TriphoneSet::const_iterator it;

			if (mdef.lines.size() < mdef.n_ci || (it = triphones.find(cur_tri)) != triphones.end())
			{
				if (mdef.lines.size() >= mdef.n_ci)
					triphones.erase(it);

				mdef.lines.push_back(ModelDefinition::Line());

				ModelDefinition::Line &line = mdef.lines.back();

				line.triphone	= cur_tri;
				line.attrib		= tokens[4];
				line.tmat_n		= atoi(tokens[5].c_str());
				line.base_sen_n	= atoi(tokens[6].c_str());
				line.left_sen_n	= atoi(tokens[7].c_str());
				line.right_sen_n= atoi(tokens[8].c_str());
				line.something_n= tokens[9];

			}
			return true;
		});
		
		if (!ok)
		{
			cout << "Mdef triphones format definition error\n";
			return false;
		}
		
		if (mdef.lines.size() < total)
		{
			cout << "Cant find triphones in mdef file:\n" << triphones;
		}


		mdef.n_tri = total - mdef.n_ci;
		mdef.n_map = total * 4;

		return true;
	}
	else
	{
		std::cout << "Cant open sentences file: " << filename << "\n";
	}
	return false;
}

ostream& operator <<(ostream& s, const SenoneMap& senmap)
{
	s << setw(4) << right << "row";
	for (int i = 0; i < 16 ; i++)
		s << setw(4) << right << hex << i;
	s << endl;

	for (int i = 0; i < senmap.size() ; i+=16)
	{
		s << setw(4) << right << hex << i;
		for (int j = 0; j < 16 && i + j < senmap.size(); j++)
			if (senmap[i + j] >= 0)
				s << setw(4) << right << senmap[i + j];
			else
				s << setw(4) << right << '-';
		s << endl;
	}
	return s;
}

ostream& operator <<(ostream& s, const Triphone& x)
{
	return s << setw(max(5, get<0>(x).size())) << utf8_to_ansi(get<0>(x))
			 << setw(4) << utf8_to_ansi(get<1>(x)) 
			 << setw(4) << utf8_to_ansi(get<2>(x))
			 << setw(2) << get<3>(x);
}

ostream& operator <<(ostream& s, const TriphoneSet& triphones)
{
	for (const auto& x : triphones)
	{
		s << x.first << endl;
	}
	return s;
}

ostream& operator <<(ostream& s, const ModelDefinition::Lines& lines)
{
	for (const auto& x : lines)
	{
		s << x.triphone 
		  << setw(7) << x.attrib
		  << setw(5) << x.tmat_n
		  << setw(7) << x.base_sen_n
		  << setw(7) << x.left_sen_n
		  << setw(7) << x.right_sen_n
		  << setw(2) << x.something_n
		  << endl;
	}
	return s;
}

template <class A, 
		  class B, 
		  class C>
void build_all_transcription(const tuple<A*,B*,C*> &args, int nword)
{
	A *words; 
	B *trans; 
	C *tri;

	tie(words, trans, tri) = args;
	if (nword < words->size())
	{
		for (auto& phonems : words->at(nword)->transcriptions)
		{
			trans->at(nword) = &phonems;
			build_all_transcription(args, nword + 1);
		}
	}
	else
		add_triphones_from_sentence(*trans, *tri);
}

bool add_triphones_from_sentence (Dictioany& dict, const list <string>& sentence, TriphoneSet& triphones)
{
	vector <DictionaryItem *> words;

	// Во-первых, добавляем все трифоны отдельных слов
	for (const string& word : sentence)
	{
		auto it = dict.find(word);

		if (it != dict.end())
		{
			DictionaryItem &item = it->second;
			words.push_back(&item);

			if (!item.added_to_triphonelist)
			{
				for (const auto& trans : item.transcriptions)
				{
					if (!add_triphones_from_transcription (trans, triphones))
					{
						cout << "Critical error: found wrong transcription " << setw(30) << left << utf8_to_ansi(word);
						return false;
					}
				}
				item.added_to_triphonelist = true;
			}
		}
		else
		{
			cout << "Critical error: cant find transcription of word " << setw(30) << left << utf8_to_ansi(word);
			return false;
		}
	}

	vector <PhonemeList *> transcripted_words(words.size());

	// Во-вторых, добавляем все трифоны возникающие на границе слов

	build_all_transcription(make_tuple(&words, &transcripted_words, &triphones), 0);

	return true;
}


void add_triphones_from_sentence(const vector <PhonemeList *>& words, TriphoneSet& triphones)
{
	string left_phone;
	string middle_phone;
	string right_phone;

	for (int i = 1; i < words.size() ; i++)
	{
		middle_phone = words[i - 1]->back();
		left_phone = sil;

		if (words[i - 1]->size() > 1)
			left_phone = *(++words[i - 1]->rbegin());	// Для слова из неск. букв, левый контекст определяется предыдущей буквой
		else if (i > 1)
			left_phone = words[i - 2]->back();		// Для слова из одной буквы, левый контекст определяется последней буквой предыдущего слова (или sil)

		right_phone = words[i]->front();

		if (words[i - 1]->size() > 1)
		{
			triphones[Triphone(middle_phone, left_phone, right_phone, 'e')];
		}
		else
		{
			triphones[Triphone(middle_phone, left_phone, right_phone, 's')];
			if (right_phone != sil)
				triphones[Triphone(middle_phone, left_phone, sil, 's')];
			if (left_phone != sil)
				triphones[Triphone(middle_phone, sil, right_phone, 's')];
		}

		middle_phone = words[i]->front();

		left_phone = words[i-1]->back();				// Для слова из неск. букв, левый контекст определяется последней буквой предыдущего слова 
		right_phone = sil;

		if (words[i]->size() > 1)
			right_phone = *(++words[i]->begin());		// Для слова из неск. букв, правый контекст определяется следующей буквой 
		else if (i < words.size() - 1)
			right_phone = words[i+1]->front();		// Для слова из одной буквы, правый контекст определяется первой буквой следующего слова (или sil)

		if (words[i]->size() > 1)
		{
			triphones[Triphone(middle_phone, left_phone, right_phone, 'b')];
		}
		else
		{
			triphones[Triphone(middle_phone, left_phone, right_phone, 's')];
			if (right_phone != sil)
				triphones[Triphone(middle_phone, left_phone, sil, 's')];
			if (left_phone != sil)
				triphones[Triphone(middle_phone, sil, right_phone, 's')];
		}
	}
}

bool add_triphones_from_transcription(const PhonemeList &trans, TriphoneSet& triphones)
{
	// При добавлении трифона в данной функции проводится учет количества
	if (trans.empty())
	{
		return false;
	}
	else if (trans.size() == 1)
	{
		triphones[Triphone(trans.front(), sil, sil, 's')] ++;
	}
	else
	{
		// Читаем транскрипцию и добавляем все трифоны в коллекцию
		auto jt = trans.begin(); 
		auto jt_next = jt; jt_next++;

		// Начальный трифон
		triphones[Triphone(*jt, sil, *jt_next, 'b')] ++;

		auto jt_prev = jt;
		jt++;
		jt_next++;

		for (; jt_next != trans.end() ; jt++, jt_next++, jt_prev++)
		{
			triphones[Triphone(*jt, *jt_prev, *jt_next, 'i')]++;
		}

		// Конечный трифон
		triphones[Triphone(*jt, *jt_prev, sil, 'e')]++;
	}
	return true;
}

bool find_triphones (const string& filename, Dictioany& dict, TriphoneSet& triphones)
{
	ifstream file(filename, ios_base::in);

	triphones.clear();
	if (file)
	{
		string			line;
		int				line_n;
		list <string>	tokens;

		std::cout << "Reading sentences and searching triphones\n";
		line_n = 0;
		while (file.good())
		{		
			getline (file, line);

			tokens.clear();

			copy(istream_iterator<string>(istringstream(line)),
				 istream_iterator<string>(),
				 back_inserter(tokens));

			if (tokens.empty())
				continue;

			if (!add_triphones_from_sentence (dict, tokens, triphones))
				return false;

			line_n ++;

			std::cout << "\rRead: " << setw(30) << left << utf8_to_ansi(line) << "\r";
		}
		std::cout << "\r" << setw(30) << " " << "\r";
		std::cout << "Readed " << line_n << " lines\n";
		return true;
	}
	else
	{
		std::cout << "Cant open sentences file: " << filename << "\n";
	}
	return false;
}

bool load_dictionary (const string& filename, Dictioany& dict)
{
	ifstream file(filename, ios_base::in);

	dict.clear();

	if (file)
	{
		string			line;
		int				line_n;
		list <string>	tokens;

		std::cout << "Reading dictionary\n";
		line_n = 0;
		while (file.good())
		{		
			getline (file, line);

			tokens.clear();

			copy(istream_iterator<string>(istringstream(line)),
				 istream_iterator<string>(),
				 back_inserter(tokens));

			if (tokens.empty())
				continue;

			string word = tokens.front();
			
			word = string(word.begin(), find(word.begin(), word.end(), '('));
			
			DictionaryItem &item = dict[word];
			item.transcriptions.push_back(PhonemeList());


			PhonemeList &phonemes = item.transcriptions.back();
			phonemes.splice(phonemes.end(), tokens, ++tokens.begin(), tokens.end());

			line_n ++;

			if (line_n % 100 == 0)
			{
				std::cout << "\rRead: " << setw(30) << left << utf8_to_ansi(word);
			}
		}

		std::cout << "\rReaded " << line_n << " lines\n";
		return true;
	}
	else
	{
		std::cout << "Cant open dictionary file: " << filename << "\n";
	}
	return false;
}

string utf8_to_ansi(const string& s)
{
    static wchar_t	wbuff[500];
    static char		ansi_buff[500];

    
	if (!MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wbuff, 500) ||
		!WideCharToMultiByte(CP_ACP, 0, wbuff, -1, ansi_buff, 500, NULL, NULL))
	{
		return "Convert error!";
	}
	else
	{
		return ansi_buff; 
	}
}

