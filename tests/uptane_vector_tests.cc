#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <boost/algorithm/hex.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

#include "config.h"
#include <string>
#include "logger.h"
#include "utils.h"

#include "uptane/uptanerepository.h"


bool match_error(Json::Value errors, Uptane::Exception * e){
    std::string repos;
    std::string messages;
    for(Json::ValueIterator it = errors.begin(); it != errors.end(); it++){
        repos += std::string(repos.size()?" or ":"") + "'" + it.key().asString() + "'";
        messages += std::string(messages.size()?" or ":"") + "'" + (*it)["error_msg"].asString() + "'";
        std::string exc_name = (*it)["error_msg"].asString();
        if(boost::starts_with(exc_name, "SecurityException") && typeid(Uptane::SecurityException) != typeid(*e)) continue;
        else if(boost::starts_with(exc_name, "TargetHashMismatch") && typeid(Uptane::TargetHashMismatch) != typeid(*e)) continue;
        else if(boost::starts_with(exc_name, "OversizedTarget") && typeid(Uptane::OversizedTarget) != typeid(*e)) continue;
        else if(boost::starts_with(exc_name, "MissingRepo") && typeid(Uptane::MissingRepo) != typeid(*e)) continue;
        

        if(it.key().asString() == e->getName()){
            if ((*it)["error_msg"].asString() == e->what()){
                return true;
            }
        }
    }
    std::cout << "Exception "<< typeid(*e).name() << "\n";
    std::cout << "Message '"<< e->what() << "' should be match: " << messages << "\n";  
    std::cout << "and Repo '"<< e->getName() << "' should be match: " << repos << "\n";  
    return false;
}

bool run_test(Json::Value vector){
    Config config;
    std::string url_director = "http://127.0.0.1:8080/"+vector["repo"].asString()+"/director/repo";
    std::string url_image = "http://127.0.0.1:8080/"+vector["repo"].asString()+"/repo/repo";
    config.uptane.director_server = url_director;
    config.uptane.repo_server = url_image;
    config.uptane.metadata_path  = "/tmp/aktualizr_repos";
    boost::filesystem::remove_all(config.uptane.metadata_path / "director");
    boost::filesystem::remove_all(config.uptane.metadata_path / "repo");

    try{
        Uptane::Repository repo(config);
        repo.updateRoot();
        repo.getNewTargets();

        //repo.refresh();
    }
    catch(Uptane::Exception e){
        if (vector["is_success"].asBool()) {
            std::cout << "Exception "<< typeid(e).name() << " happen, but shouldn't\n";
            std::cout << "exception message: " << e.what() << "\n";
            return false;
        }
        return match_error(vector["errors"], &e);

    }catch(...){
        //std::cout << "Exception "<< typeid(e).name() << " happen, but shouldn't\n";
        std::cout << "Undefined exception\n";
        return false;
    }
    
    
    if (vector["is_success"].asBool()){
        return true;
        
    }
    else{
        std::cout << "No exceptions, but expects\n";
        return false;
    }
}

int main(int argc, char*argv[]) {
  loggerInit();
  loggerSetSeverity(LVL_maximum);
  boost::filesystem::path full_path( boost::filesystem::current_path() );
    std::cout << "Current path is : " << full_path << std::endl;

  
  if (argc >= 2) {
    std::string command = std::string(argv[1]) + "/server.py &";
    std::cout << "command: " << command << "\n";
    system(command.c_str());
    sleep(3);
  }
  Json::Value json_vectors = Utils::parseJSONFile("tests/uptane-test-vectors/vectors/vector-meta.json");
  int passed = 0;
  int failed = json_vectors.size();
  for(Json::ValueIterator it = json_vectors.begin(); it != json_vectors.end(); it++){
     std::cout << "Running testvector " <<(*it)["repo"].asString() << "\n";
     bool pass = run_test(*it);
     passed += (int)pass;
     failed -= (int)pass;
     std::string result = (pass == true)?"PASS\n":"FAIL\n";
     std::cout << "TEST: "+result;
  }
  std::cout << "\n\n\nPASSED TESTS: "<< passed << "\n";
  std::cout << "FAILED TESTS: "<< failed << "\n";
  system("killall python3");
  return (int)failed;
}

