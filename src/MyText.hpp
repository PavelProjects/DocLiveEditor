#pragma one
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
using namespace std;

char znak_prep[] = "!?.";

class Text{ //добавить разбиение на строки по точкам, а не только по переносу строки
    private:
        vector<string> text;
    public:
        int size(){
            return text.size();
        }
        string at_st(int l){
            if(l < text.size()){
                return text.at(l);
            }
            return "";
        }
        void push_back(string n){
            text.push_back(n);
        }
        void insert(int p, string n){
            if(p < text.size()){
                text.insert(text.begin() + p, n);
            }
        }
        void erase(int p){
            if(p < text.size()){
                text.erase(text.begin() + p);
            }
        }
        void pop_back_string(){
            text.pop_back();
        }
        char at_c(int s, int p){
            if(s < text.size()){
                if(p < text.at(s).size()){
                    return text.at(s).at(p);
                }
            }
            return 0;
        }
        void push_back(int s, char c){
            if(text.size() == 0){
                string n;
                text.push_back(n);
            }
            if(s < text.size()){
                if(text.at(s).back() == '\n'){
                    text.at(s).insert(text.at(s).begin() + text.at(s).length()-1, c);
                }else{
                    text.at(s).push_back(c);
                }
            }
            if(c == '\n'){
                string n;
                text.push_back(n);
            }
        }
        void insert(int p, int w, char c){
            if(text.size() == 0){
                string n;
                text.push_back(n);
            }
            if(text.size() > p){
                if(text.at(p).size() > w){
                    text.at(p).insert(text.at(p).begin() + w, c);
                    if(c == '\n' && text.at(p).back() != '\n'){
                        string n = text.at(p).substr(w+1, text.at(p).size());
                        text.at(p) = text.at(p).substr(0, w+1);
                        text.insert(text.begin() + p + 1, n);
                    }
                }else{
                    push_back(p, c);
                }
            }
        }
        void erase(int s, int p){
            if(s < text.size()){
                if(p < text.at(s).size()){
                    if(p == 0 && text.at(s).length() != 0 && text.size() > 1 && s > 1){
                        if(text.at(s).back() == '\n'){
                            if(text.at(s).length() > 1){
                                text.at(s-1) += text.at(s);
                            }
                            text.erase(text.begin() + s); 
                        }else{
                            text.at(s-1) += text.at(s);
                            text.erase(text.begin() + s); 
                        }
                    }else if(text.at(s).size() == 0){
                        if(text.size() > 1){
                            text.erase(text.begin() + s);
                        }
                    }else{
                        text.at(s).erase(text.at(s).begin() + p);
                    }
                }
            }
        }
        void pop_back_c(int s){
            if(s < text.size()){
                if(text.at(s).size() == 0){
                    if(text.size() > 1){
                        text.erase(text.begin() + s);
                    }
                }else{
                    text.at(s).pop_back();
                }
            }
        }
        void append_with_prev(int s){
            if(s < text.size() && text.size() != 1){
                if(text.at(s-1).back() == '\n'){
                    text.at(s-1).pop_back();
                }
                text.at(s-1)+=text.at(s);
                text.erase(text.begin() + s);
            }
        }
        bool loadFromFile(string path){
            ifstream file(path, ios::in|ios::binary|ios::ate);
            if(file.is_open()){
                int size = file.tellg();
                char* lod = new char [size];
                file.seekg (0, ios::beg);
                file.read (lod, size);
                file.close();
                string n;
                for(int i=0; i < size; i++){
                    n.push_back(lod[i]);
                    if(lod[i] == '\n'){
                        text.push_back(n);
                        n.clear();
                    }
                }
                if(n.back() != '\n') text.push_back(n);
                cout<<"Loaded"<<endl;
                delete[] lod;
                return true;
            }else{
                cout<<"Can't open file!"<<endl;
                return false;
            }
            return false;
        }
        bool writeToFile(string path){
            ofstream file(path);
            if(file.is_open()){
                for(int i = 0; i< text.size(); i++){
                    file<<text.at(i);
                }
                file.close();
                return true;
            }else{
                cout<<"Can't open file for write!"<<endl;
                return false;
            }
        }
};