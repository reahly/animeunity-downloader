#include <iostream>
#include <cpr/cpr.h>
#include <boost/regex.hpp>
#include "html_coder.hpp"
#include <nlohmann/json.hpp>
#include <BS_thread_pool.hpp>
#include <regex>

std::string split_regex( const std::string& in, const boost::regex& regex, const int count ) {
	const boost::regex& rx( regex );
	boost::smatch matched_rx;
	if ( !regex_search( in, matched_rx, rx ) )
		return "";

	return matched_rx.str( count );
}

std::string split_regex( const std::string& in, const std::string& regex, const int count ) {
	const boost::regex rx( regex );
	boost::smatch matched_rx;
	if ( !regex_search( in, matched_rx, rx ) )
		return "";

	return matched_rx.str( count );
}

std::string sanitize_folder_name( const std::string& filename ) {
	const std::unordered_set illegalChars = {
		 '\\', '<', '>', '*', '?', '/', '$',
		'\'', '\"', ':', '@', '`', '|',
	};

	std::string sanitized;

	for ( char ch : filename ) {
		if ( !illegalChars.contains( ch ) ) {
			sanitized += ch;
		}
	}

	return sanitized;
}

int main( ) {
	cpr::Header header{
		{
			"User-Agent",
			"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
		}
	};


	std::string id_anime;
	printf( "Inserisci id dell anime: " );
	std::getline( std::cin, id_anime );
	std::cout << '\n';

	struct episodes_info_t {
		std::string title, video_url, file_name;
		unsigned int id, scws_id;
	};

	std::vector<episodes_info_t> episodes_info;

	int total_episodes = 0;
	int current_start_range = 1;
	int current_end_range = 120;
	do {
		const auto req = Get( cpr::Url{ std::format( "https://www.animeunity.to/info_api/{}/1?start_range={}&end_range={}", id_anime, current_start_range, current_end_range ) }, header );
		if ( req.error.code != cpr::ErrorCode::OK || !nlohmann::json::accept( req.text ) )
			continue;

		const auto req_parsed = nlohmann::json::parse( req.text );

		std::string title;
		if ( !req_parsed.at( "name" ).is_null( ) )
			title = sanitize_folder_name( req_parsed.at( "name" ).get<std::string>( ) );
		else
			title = sanitize_folder_name( req_parsed.at( "slug" ).get<std::string>( ) );

		std::cout << "Title: " << title << '\n';

		total_episodes = req_parsed.at( "episodes_count" ).get<int>( );
		for ( const auto& episodes : req_parsed.at( "episodes" ).items( ) ) {
		redo:
			const auto file_name = std::filesystem::path( episodes.value( ).at( "file_name" ).get<std::string>( ) ).stem( ).string( );
			if ( std::filesystem::exists( std::string( R"(\\bignigga\data\media\series-anime\)" ).append( file_name ).append( ".mkv" ) ) )
				continue;

			const auto id = episodes.value( ).at( "id" ).get<unsigned int>( );
			const auto scws_id = episodes.value( ).at( "scws_id" ).get<unsigned int>( );

			const auto embed_url_req = Get( cpr::Url{ std::format( "https://www.animeunity.to/embed-url/{}", id ) }, header );
			if ( embed_url_req.error.code != cpr::ErrorCode::OK ) {
				printf( "redo 01 \n" );
				goto redo;
			}

			const auto playlist_embed_req = Get( cpr::Url{ embed_url_req.text }, header );
			if ( playlist_embed_req.error.code != cpr::ErrorCode::OK ) {
				printf( "redo 02 \n" );
				goto redo;
			}

			const auto main_token = split_regex( playlist_embed_req.text, R"('token': '(.*?)')", 1 );
			const auto expires = split_regex( playlist_embed_req.text, R"('expires': '(.*?)')", 1 );
			const auto vix_url = split_regex( playlist_embed_req.text, R"(url: '(.*?)')", 1 );

			episodes_info.emplace_back( episodes_info_t{ title, std::format( "{}?token={}&expires={}&h=1", vix_url, main_token, expires ), file_name, id, scws_id } );
		}

		current_start_range += 120;
		current_end_range += 120;
	}
	while ( episodes_info.size( ) != total_episodes - 1 && episodes_info.size( ) != total_episodes );

	if ( episodes_info.empty( ) )
		return -1;

	BS::thread_pool thread_pool( 15 );
	for ( const auto& a : episodes_info ) {
		thread_pool.submit_task( [a]( ){
		redo:
			SHELLEXECUTEINFOA ShExecInfo = { };
			ShExecInfo.cbSize = sizeof( SHELLEXECUTEINFOA );
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.hwnd = nullptr;
			ShExecInfo.lpVerb = nullptr;
			ShExecInfo.lpFile = R"(N_m3u8DL-RE.exe)";
			std::string parameters = std::format( R"(--no-log -sv best -sa best -mt true -M format=mkv --save-dir "\\bignigga\data\media\series-anime\{}" --save-name "{}" "{}")", a.title, a.file_name, a.video_url );
			ShExecInfo.lpParameters = parameters.c_str( );
			ShExecInfo.lpDirectory = R"(C:\Users\PC\Downloads\N_m3u8DL-RE\)";
			ShExecInfo.nShow = SW_HIDE;
			ShExecInfo.hInstApp = nullptr;
			ShellExecuteExA( &ShExecInfo );
			WaitForSingleObject( ShExecInfo.hProcess, INFINITE );

			if ( !std::filesystem::exists( std::format( R"(\\bignigga\data\media\series-anime\{}\{}.mkv)", a.title, a.file_name ) ) ) {
				goto redo;
			}

			CloseHandle( ShExecInfo.hProcess );
		} );
	}

	thread_pool.wait( );

	printf( "episodes info: %i \n", episodes_info.size( ) );
	system( "pause" );
}
