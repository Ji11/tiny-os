//
// Created by OrangeJuice on 2023/5/29.
//
#include "user.h"
#include "fcb.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>

#define userDataAddress 1048576 // �û���������ʼ��ַ
#define maxBlockCount 9216  // ������
#define maxUserCount 10 // ����û���

#define modifedTimesLength 7
#define userLength (25+4*2)*10  // 4*2 ���ڻ��з�
#define fatLength (48+8*2)*9216
#define bitMapLength (1+2)*9216

// �洢Ŀ¼�ṹ
vector<int> dirStack;
vector<int> catalogStack;   //way
int currentCatalog = 0; // ��ǰĿ¼�� FCB ��
vector<int> filesInCatalog; // ��ǰĿ¼�µ��ļ�

using namespace std;

/*
 * user
 * int isused; // 1λ���Ƿ�ʹ��
    string username; // 12λ���û���
    string password; //��� 8 λ������ĸ�������
    int root; // 4λ���û���Ŀ¼ FCB
 * */
user *user;
int nowUser = -1;    // ��ǰ�û�

// �߳� 1���������߳�
thread::id cmd;
// �߳� 2���ں��߳�
thread::id kernels;

int modifedTimes = 0;   // �޸Ĵ���
int message = 0;    // ����ʶ�����ĸ�ָ��
string argument;   // ���ڴ洢ָ��Ĳ���
int *fatBlock;  // fatBlock
int *bitMap;    // bitMap
string currentPath = "czh";    // ��ǰ·��

/*
 * fcb
 *  int isused; // 1λ���Ƿ�ʹ��
    int isHide; // 1λ���Ƿ�����
    string name; // 20λ���ļ���
    int type;	// 1λ����� 0���ļ� 1��Ŀ¼
    int user;	// 1λ���û� 0��root 1��user
    int size;	// 7λ���ļ���С
    int address; // 4λ�������ַ
    string modifyTime; // 14λ���޸�ʱ��
 * */
fcb *fcbs;  // fcb
bool isLogin = false;   // �Ƿ��¼

// ����λ���洢��������Ҫ��Ϊ 4 λ������ֻ�� 2 λ����ô����ǰ�油 0
string fillFileStrins(string str, int len) {
    int length = str.length();
    if (str.length() < len) {
        for (int i = 0; i < len - length; i++) {
            str += "%";
        }
    }
    return str;
}

// int ת string
string intToString(int num, int len) {
    string str = to_string(num);
    return fillFileStrins(str, len);
}

// string ת int
int stringToInt(const string &str) {
    return strtol(str.c_str(), nullptr, 10);
}

string getTrueFileStrings(string s){
    string str = "";
    for (int i = 0; i < s.length(); i++) {
        if (s[i] != '%') {
            str += s[i];
        }
    }
    return str;
}

// ��ʼ��������Ϣ
void os::initTemps() {
    fatBlock = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        fatBlock[i] = 0;
    }
    bitMap = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        bitMap[i] = 0;
    }
    fcbs = new fcb[maxBlockCount];
    user = new class user[maxUserCount];
    message = 0;
    argument = "";
    nowUser = -1;
}

// ��ȡ��ǰʱ�䣬���Ϊ 202305301454 ����ʽ
string getCurrentTime() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    string year = intToString(1900 + ltm->tm_year, 4);
    string month = intToString(1 + ltm->tm_mon, 2);
    string day = intToString(ltm->tm_mday, 2);
    string hour = intToString(ltm->tm_hour, 2);
    string min = intToString(ltm->tm_min, 2);
    return year + month + day + hour + min;
}

// �����û���Ϣ
bool os::saveUserToFile(int u) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + u * 33, ios::beg);
    file << user[u].isused << endl;
    file << fillFileStrins(user[u].username, 12) << endl;
    file << fillFileStrins(user[u].password, 8) << endl;
    file << fillFileStrins(intToString(user[u].root, 4), 4) << endl;
    file.close();
    return true;
}

// ���� fatBlock ��Ϣ
bool os::saveFatBlockToFile() {
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + userLength, ios::beg);
    for (int i = 0; i < maxBlockCount; i++) {
        file << intToString(fatBlock[i], 4) << endl;
    }
    file.close();
    return true;
}

// ���� bitMap ��Ϣ
bool os::saveBitMapToFile() {
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + userLength + fatLength, ios::beg);
    for (int i = 0; i < maxBlockCount; i++) {
        file << bitMap[i] << endl;
    }
    file.close();
    return true;
}

bool os::saveFcbToFile(int f) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + userLength + fatLength + bitMapLength + 64 * f, ios::beg);
    file << fcbs[f].isused << endl;
    file << fcbs[f].isHide << endl;
    file << fillFileStrins(fcbs[f].name, 20) << endl;
    file << fcbs[f].type << endl;
    file << fcbs[f].user << endl;
    file << fcbs[f].size << endl;
    file << fcbs[f].address << endl;
    file << fillFileStrins(fcbs[f].modifyTime, 12) << endl;
    file.close();
    return true;
}

int os::getEmptyFcb() {
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].isused == 0) {
            return i;
        }
    }
    return -1;
}

// ��ȡ���� Block
int os::getEmptyBlock() {
    for (int i = 0; i < maxBlockCount; i++) {
        if (bitMap[i] == 0) {
            bitMap[i] = 1;
            return i;
        }
    }
    return -1;
}

// �������������ڱ����޸Ĺ����ļ�ϵͳ
bool os::saveFileSys(int f, string content) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    // ��λ�� fcbs ��ʼ��λ��
    file.seekg(modifedTimesLength + userLength + fatLength + bitMapLength + 64 * f, ios::beg);
    file << fcbs[f].isused << endl;
    file << fcbs[f].isHide << endl;
    file << fillFileStrins(fcbs[f].name, 20) << endl;
    file << fcbs[f].type << endl;
    file << fcbs[f].user << endl;
    file << fcbs[f].size << endl;
    file << fcbs[f].address << endl;
    file << fillFileStrins(fcbs[f].modifyTime, 12) << endl;
    // ��λ���û����ݿ�
    file.seekg(userDataAddress + fcbs[f].address * 1026, ios::beg);  // 1024 + 2�����з���
    int blockNum = fcbs[f].size / 1024;   // ���ڼ�¼��ǰ�ļ���Ҫ���ٸ���
    string temp;    // ���ڼ�¼��ǰ�������
    int addressPointer = 0;  // ���ڼ�¼��ǰ��ĵ�ַָ��
    int nowAddress = fcbs[f].address;   // ���ڼ�¼��ǰ��ĵ�ַ
    while (true) {
        if (blockNum <= 0) {
            break;
        }
        int size = 0;   // ���ڼ�¼��ǰ��Ĵ�С
        int big = 0;
        int i = addressPointer;   // ���ڼ�¼��ǰ��ĵ�������
        while (true) {
            if (content[i] == '\n') {
                big += 2;
            } else {
                big += 1;
            }
            i++;
            if (i == addressPointer + 1024) {
                break;
            }
        }
        temp = content.substr(addressPointer, big);
        addressPointer += big;
        blockNum--;
        file << temp << endl;
        fatBlock[nowAddress] = getEmptyBlock();
        if (fatBlock[nowAddress] == -1) {
            fatBlock[nowAddress] = 0;
            return false;
        }
        nowAddress = fatBlock[nowAddress];
        file.seekg(userDataAddress + nowAddress * 1026, ios::beg);
    }
    temp = content.substr(addressPointer, content.size() - addressPointer);
    file << temp;

    int size = 0;
    int big = 0;
    int j = addressPointer;
    while (true) {
        if (content[j] == '\n') {
            big += 2;
        } else {
            big += 1;
        }
        j++;
        if (j == content.size()) {
            break;
        }
    }
    for (int i = 0; i < 1024 - big; i++) {
        file << "%";
    }
    file << endl;
    file.close();
    saveFatBlockToFile();
    saveBitMapToFile();
    return true;
}

bool os::saveFileSys(int f, vector<int> content) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    // ��λ�� fcbs ��ʼ��λ��
    file.seekg(modifedTimesLength + userLength + fatLength + bitMapLength + 64 * f, ios::beg);
    file << fcbs[f].isused << endl;
    file << fcbs[f].isHide << endl;
    file << fillFileStrins(fcbs[f].name, 20) << endl;
    file << fcbs[f].type << endl;
    file << fcbs[f].user << endl;
    file << fcbs[f].size << endl;
    file << fcbs[f].address << endl;
    file << fillFileStrins(fcbs[f].modifyTime, 12) << endl;
    // ��λ���û����ݿ�
    file.seekg(userDataAddress + fcbs[f].address * 1026, ios::beg);  // 1024 + 2�����з���
    int blockNum = fcbs[f].size / 1024;   // ���ڼ�¼��ǰ�ļ���Ҫ���ٸ���
    string temp;    // ���ڼ�¼��ǰ�������
    int addressPointer = 0;  // ���ڼ�¼��ǰ��ĵ�ַָ��
    int nowAddress = fcbs[f].address;   // ���ڼ�¼��ǰ��ĵ�ַ
    while (true) {
        if (blockNum <= 0) {
            break;
        }
        int max = addressPointer + 1024;
        for (int i = addressPointer; i < max; i++) {
            file << intToString(content[i], 4) << "%";
        }
        file << "     " << endl;
        blockNum--;
        fatBlock[nowAddress] = getEmptyBlock();
        if (fatBlock[nowAddress] == -1) {
            fatBlock[nowAddress] = 0;
            return false;
        }
        nowAddress = fatBlock[nowAddress];
        file.seekg(userDataAddress + nowAddress * 1026, ios::beg);
    }
    for (int i = addressPointer; i < content.size(); i++) {
        file << intToString(content[i], 4) << "%";
    }
    for (int i = 0; i < 1024 + (addressPointer - content.size()) * 5; i++) {
        file << "%";
    }
    file << endl;
    file.close();
    saveFatBlockToFile();
    saveBitMapToFile();
    return true;
}

// ɾ���ļ��е���Ϣ
bool os::deleteFileSystemFile(int f) {
    int n = fcbs[f].address;
    vector<int> changes;
    while (true) {
        changes.push_back(n);
        bitMap[n] = 0;
        n = fatBlock[n];
        if (fatBlock[n] == 0) {
            break;
        }
    }
    for (int i = 0; i < changes.size(); i++) {
        fatBlock[changes[i]] = 0;
    }
    for (int i = 2; i < catalogStack.size(); i++) {
        fcbs[catalogStack[i]].modifyTime = getCurrentTime();
        saveFcbToFile(catalogStack[i]);
    }
    fcbs[f].reset();
    saveFcbToFile(f);
    saveFatBlockToFile();
    saveBitMapToFile();
    return true;
}

// ����Ŀ¼������Ϊ�û����
int os::makeDirectory(int u) {
    if (u != -1) {
        int voidFcb = getEmptyFcb();
        if (voidFcb == -1) {
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }
        fcbs[voidFcb].isused = 1;
        fcbs[voidFcb].isHide = 0;
        fcbs[voidFcb].type = 1;
        fcbs[voidFcb].user = u;
        fcbs[voidFcb].size = 0;
        fcbs[voidFcb].address = getEmptyBlock();
        fcbs[voidFcb].modifyTime = getCurrentTime();
        fcbs[voidFcb].name = "root";
        // �û����ݿ鲻��
        if (fcbs[voidFcb].address == -1) {
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }
        user[nowUser].root = voidFcb;

        // ���� README.txt
        int voidFcbFile = getEmptyFcb();
        if (voidFcbFile == -1) {
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }
        fcbs[voidFcbFile].isused = 1;
        fcbs[voidFcbFile].isHide = 0;
        fcbs[voidFcbFile].type = 0;
        fcbs[voidFcbFile].user = u;
        fcbs[voidFcbFile].size = 0;
        fcbs[voidFcbFile].address = getEmptyBlock();
        fcbs[voidFcbFile].modifyTime = getCurrentTime();
        fcbs[voidFcbFile].name = "README.txt";
        // �û����ݿ鲻��
        if (fcbs[voidFcbFile].address == -1) {
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }

        // Ŀ¼��
        vector<int> stack;
        stack.push_back(voidFcbFile);
        if (!saveFileSys(voidFcb, stack)) {
            cout << "Error: Can't create directory!" << endl;
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            deleteFileSystemFile(voidFcbFile);
            return -1;
        } else {
            cout << "Create directory successfully!" << endl;
        }
        string info = "This is temporary README.txt.";
        if (!saveFileSys(voidFcbFile, info)) {
            cout << "Error: Can't create directory!" << endl;
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            deleteFileSystemFile(voidFcbFile);
            return -1;
        } else {
            cout << "Create README.txt successfully!" << endl;
        }
        user[u].root = voidFcb;
        return voidFcb;
    } else {
//        cout << "Please input the name of the directory: ";
//        string dirName;
//        while (cin>>dirName){
//            if (dirName == "root"){
//                cout << "Error: Can't create root directory!" << endl;
//                cout << "Please input the name of the directory: ";
//                continue;
//            }
//            if (dirName.size() > 20) {
//                cout << "Error: Directory name is too long!" << endl;
//                cout << "Please input the name of the directory: ";
//                continue;
//            }
//            bool flag = false;
//            for (int i=0;i< dirStack.size();i++){
//                if (fcbs[dirStack[i]].name == dirName && fcbs[dirStack[i]].type == 1){
//                    flag = true;
//                }
//            }
//            if (flag){
//                cout << "Error: Directory name is already exist!" << endl;
//                cout << "Please input the name of the directory: ";
//                continue;
//            }
//            break;
//        }
//
//        int voidFcb = getEmptyFcb();
//        if (voidFcb == -1) {
//            cout << "Error: No space for new directory!" << endl;
//            return -1;
//        }
//        fcbs[voidFcb].isused = 1;
//        fcbs[voidFcb].isHide = 0;
//        fcbs[voidFcb].name = dirName;
//        fcbs[voidFcb].type = 1;
//        fcbs[voidFcb].user = u;
//        fcbs[voidFcb].size = 0;
//        fcbs[voidFcb].address = getEmptyBlock();
//        fcbs[voidFcb].modifyTime = getCurrentTime();
//        vector
    }
}

void os::displayFileInfo(){
    cout << "���\t" << "�ļ���\t\t" << "����" << "\t" << "�ɼ���\t" <<"Ȩ��\t" << "��С\t" << "����޸�ʱ��\t" << endl;
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].user == nowUser)
            cout << i << "\t" << fcbs[filesInCatalog[i]].name << "\t\t"
                 << ((fcbs[filesInCatalog[i]].type == 0) ? "file" : "dir")
                 << "\t" << ((fcbs[filesInCatalog[i]].isHide == 0) ? "�ɼ�" : "����")
                 << "\t" << ((fcbs[filesInCatalog[i]].size == 0) ? fcbs[filesInCatalog[i]].size : -1)
                 << "\t" << fcbs[filesInCatalog[i]].modifyTime << "\t" << endl;
    }
    cout << endl;
}

// ���ļ��ж�ȡָ�� FCB ������
vector<int> os::getFcbs(int fcb) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::in | ios::out);
    if (!file.is_open()) {
        cout << "Error: Can't open the disk!" << endl;
        vector<int> error;
        return error;
    }
    string data = "";
    string temp;
    int fcbAddress = fcbs[fcb].address;
    while(true){
        file.seekg(userDataAddress + fcbAddress * 1024, ios::beg);
        if(fatBlock[fcbAddress] == 0){
            break;
        }
        file >> temp;
        data += temp;
        fcbAddress = fatBlock[fcbAddress];  // ��һ�����ݿ�
    }
    file>>temp;
    data += temp;
    istringstream iss(data);
    string s;
    vector<int> res;
    while (getline(iss, s, '%')) {
        res.push_back(strtol(s.c_str(), nullptr, 10));
    }
    file.close();
    return res;
}

// ʵ���û�ע���߼������û���Ϣд���ļ�
void os::userRegister() {
    int u = -1;
    // ���ҿհ׿ռ�
    for (int i = 0; i < 10; i++) {
        if (user[i].isused == 0) {
            u = i;
            break;
        }
    }
    if (u == -1) {
        cout << "Error: No space for new user!" << endl;
        return;
    }
    cout<<"* Welcome to register!You should input your username and password.The username should be less than 20 characters and only contains letters and numbers.And the password should be less than 8 characters."<<endl;
    cout << "Please input your username: ";
    string username;
    while (cin >> username) {
        if (username.size() > 20) {
            cout << "Error: Username is too long!" << endl;
            cout << "Please input your username: ";
            continue;
        }
        // �ж��û��Ƿ��Ѿ�����
        for (int i = 0; i < 10; i++) {
            if (user[i].username == username) {
                cout << "Error: Username is already exist!" << endl;
                continue;
            }
        }
        bool flag = true;
        for (int i = 0; i < username.size(); i++) {
            if (username[i] < 48 || (username[i] > 57 && username[i] < 65) || (username[i] > 90 && username[i] < 97) ||
                username[i] > 122) {
                flag = false;
                break;
            }
        }
        if (flag) {
            break;
        } else {
            cout << "Error: Username is not valid!" << endl;
            cout << "Please input your username: ";
        }
    }

    user[u].username = username;
    cout << "Please input your password: ";
    string password;
    while (cin >> password) {
        if (password.size() > 8) {
            cout << "Error: Password is too long!" << endl;
            cout << "Please input your password: ";
            continue;
        }
        bool flag = true;
        for (int i = 0; i < password.size(); i++) {
            if (password[i] < 48 || (password[i] > 57 && password[i] < 65) || (password[i] > 90 && password[i] < 97) ||
                password[i] > 122) {
                flag = false;
                break;
            }
        }
        if (flag) {
            break;
        } else {
            cout << "Error: Password is not valid!" << endl;
            cout << "Please input your password: ";
        }
    }
    user[u].password = password;
    user[u].isused = 1;
    user[u].root = 0;
    cout << "*** User " << user[u].username << " regist successfully!" << endl;
    saveUserToFile(u);
    makeDirectory(u);
}

// ʵ���û���¼�߼�
void os::userLogin() {
    int u = -1;
    cout << "Input \"--register\" to register a new user." << endl;
    cout << "Please input your username: ";
    string username;
    cin >> username;
    if (username == "--register") {
        userRegister();
        return;
    }
    for (int i = 0; i < 10; i++) {
        if (user[i].username == username) {
            u = i;
        }
    }
    if (u == -1) {
        cout << "Error: Username is not exist!" << endl;
        cout << "You can register first!" << endl;
        return;
    }
    cout << "Please input your password: ";
    string password;
    cin >> password;
    if (user[u].password == password) {
        nowUser = u;
        dirStack.push_back(u);
        currentCatalog = user[u].root;
        catalogStack.push_back(currentCatalog);
        filesInCatalog = getFcbs(user[u].root);
        cout<<"* Welcome "<<user[u].username<<"!"<<endl;
        displayFileInfo();
        isLogin = true;
    } else {
        cout << "Error: Password is not correct!" << endl;
        return;
    }
}

void os::createFileSys() {
    thread::id id = this_thread::get_id();
    fstream file;   // �ļ���
    // �ļ��� cmake-build-debug/ ��
    file.open("disk.txt", ios::out);    // ���ļ�
    if (!file) {
        cout << "Error: Can't open file!" << endl;
        exit(1);
    }
    cout << "*** Creating file system..." << endl;
    modifedTimes = 0;   // �޸Ĵ���
    file << "*****" << endl; // �޸Ĵ���
    // д���ʼ�����û���Ϣ
    for (int i = 0; i < 10; i++) {
        file << user[i].isused << endl;
        file << fillFileStrins(user[i].username, 12) << endl;
        file << fillFileStrins(user[i].password, 8) << endl;
        file << fillFileStrins(intToString(user[i].root, 4), 4) << endl;
    }
    // д���ʼ���� fat block ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file << intToString(fatBlock[i], 4) << endl;
    }
    // д���ʼ���� bit map ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file << bitMap[i] << endl;
    }
    // д���ʼ���� fcb ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file << fcbs[i].isused << endl;
        file << fcbs[i].isHide << endl;
        file << fillFileStrins(fcbs[i].name, 20) << endl;
        file << fcbs[i].type << endl;
        file << fcbs[i].user << endl;
        file << fcbs[i].size << endl;
        file << intToString(fcbs[i].address, 4) << endl;
        file << fillFileStrins(fcbs[i].modifyTime, 12) << endl;
    }
    // ��λ���û�����
    file.seekg(userDataAddress, ios::beg);
    file << endl;
    // д���ʼ�����û�����
    for (int i = 0; i < maxBlockCount; i++) {
        // 1024 �� 0
        for (int j = 0; j < 1024; j++) {
            file << "0";
        }
        file << endl;
    }
    file.close();
}

// �����ļ���Ϣ
void os::initFileSystem() {
    fstream file;
    string ss;
    cout << "*** Reading file system..." << endl;
    cout << ss << endl;
    file.open("disk.txt", ios::in | ios::out);  // ��дģʽ���ļ�
    if (!file.is_open()) {
        createFileSys();
        return;
    }
    cout << "success" << endl;
    file >> ss;
    modifedTimes = stringToInt(ss);    // �޸Ĵ���
    // �����û���Ϣ
    for (int i = 0; i < 10; i++) {
        file >> ss;
        user[i].isused = stringToInt(ss);
        file >> ss;
        user[i].username = getTrueFileStrings(ss);
        file >> ss;
        user[i].password = getTrueFileStrings(ss);
        file >> ss;
        user[i].root = stringToInt(ss);
    }
    // ���� fat block ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        fatBlock[i] = strtol(ss.c_str(), nullptr, 10);  // ���������ֱ�Ϊ���ַ�����ָ�룬���ƣ���˼�ǽ��ַ���ת��Ϊ 10 ���Ƶ�����
    }
    // ���� bit map ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        bitMap[i] = stringToInt(ss);
    }
    // ���� fcb ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        fcbs[i].isused = stringToInt(ss);
        file >> ss;
        fcbs[i].isHide = stringToInt(ss);
        file >> ss;
        fcbs[i].name = getTrueFileStrings(ss);
        file >> ss;
        fcbs[i].type = stringToInt(ss);
        file >> ss;
        fcbs[i].user = stringToInt(ss);
        file >> ss;
        fcbs[i].size = stringToInt(ss);
        file >> ss;
        fcbs[i].address = stringToInt(ss);
        file >> ss;
        fcbs[i].modifyTime = getTrueFileStrings(ss);
    }
    file.close();
}

// ����������kernel �̺߳� run �̹߳���
os::os() : ready(false) {
    cout << "*** Preparing for the system..." << endl;
    fatBlock = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        fatBlock[i] = 0;
    }
    bitMap = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        bitMap[i] = 0;
    }
    fcbs = new fcb[maxBlockCount];
    user = new class user[10];  // class �������� user ���ͺ� user ��
    message = 0;    // ����ʶ�����ĸ�ָ��, 0 ������ָ��
}

void os::reset() {
    fatBlock = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        fatBlock[i] = 0;
    }
    bitMap = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        bitMap[i] = 0;
    }
    fcbs = new fcb[maxBlockCount];
    user = new class user[10];
}

os::~os() {
//    std::cout << "os destructor" << std::endl;
}


// create ��������ļ�
void os::createFile(const string &filename, int type) {
    // �ж��Ƿ��������ļ�
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].name == filename && fcbs[i].isused == 1) {
            cout << "Error: File already exists" << endl;
            return;
        }
    }
    // �ж��Ƿ��п��� fcb
    int fcbIndex = -1;
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].isused == 0) {
            fcbIndex = i;
            break;
        }
    }
    if (fcbIndex == -1) {
        cout << "Error: No more space for new file" << endl;
        return;
    }
    // �ж��Ƿ��п��� block
    int blockIndex = -1;
    for (int i = 0; i < maxBlockCount; i++) {
        if (bitMap[i] == 0) {
            blockIndex = i;
            break;
        }
    }
    if (blockIndex == -1) {
        cout << "Error: No more space for new file" << endl;
        return;
    }
    // �����ļ�
    fcbs[fcbIndex].isused = 1;
    fcbs[fcbIndex].isHide = 0;
    fcbs[fcbIndex].name = filename;
    fcbs[fcbIndex].type = type;

    fcbs[fcbIndex].user = nowUser;
    fcbs[fcbIndex].size = 0;
    fcbs[fcbIndex].address = blockIndex;
    fcbs[fcbIndex].modifyTime = getCurrentTime();
    bitMap[blockIndex] = 1;

    cout << "File created successfully" << endl;
}

void os::deleteFile(const string &filename) {
    // �ж��Ƿ��и��ļ�
    int fcbIndex = -1;
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].name == filename && fcbs[i].isused == 1) {
            fcbIndex = i;
            break;
        }
    }
    if (fcbIndex == -1) {
        cout << "Error: No such file" << endl;
        return;
    }
    // �ж��Ƿ���Ȩ��
    if (fcbs[fcbIndex].user != nowUser && nowUser != 0) {
        cout << "Error: Permission denied" << endl;
        return;
    }
    // ɾ���ļ�
    int blockIndex = fcbs[fcbIndex].address;
    bitMap[blockIndex] = 0;
    fcbs[fcbIndex].isused = 0;
    fcbs[fcbIndex].isHide = 0;
    fcbs[fcbIndex].name = "";
    fcbs[fcbIndex].type = 0;
    fcbs[fcbIndex].user = 0;
    fcbs[fcbIndex].size = 0;
    fcbs[fcbIndex].address = 0;
    fcbs[fcbIndex].modifyTime = "";


    cout << "File deleted successfully" << endl;
}

void os::showFileList() {
    cout << "File list:" << endl;
    cout << "Name\t\tType\t\tSize\t\tModify time" << endl;
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].isused == 1 && fcbs[i].isHide == 0) {
            cout << fcbs[i].name << "\t\t" << fcbs[i].type << "\t\t" << fcbs[i].size << "\t\t" << fcbs[i].modifyTime
                 << endl;
        }
    }
}

void os::cd(const string &filename) {
    if (filename == "..") {
        if (currentPath == "czh") {
            cout << "Error: Already in root directory" << endl;
            return;
        }
        currentPath = currentPath.substr(0, currentPath.length() - 1);
        currentPath = currentPath.substr(0, currentPath.find_last_of("/") + 1);
        return;
    }
    if (filename == "~") {
        currentPath = "czh";
        return;
    }
    if (filename == "") {
        cout << "Error: No such directory" << endl;
        return;
    }
    if (filename[0] == '/') {
        if (filename == "/") {
            currentPath = "czh";
            return;
        }
        string temp = filename.substr(1, filename.length() - 1);
        if (temp.find_first_of("/") != string::npos) {
            cout << "Error: No such directory" << endl;
            return;
        }
        for (int i = 0; i < maxBlockCount; i++) {
            if (fcbs[i].isused == 1 && fcbs[i].isHide == 0 && fcbs[i].type == 0 && fcbs[i].name == temp) {
                currentPath = "/" + temp + "/";
                return;
            }
        }
        cout << "Error: No such directory" << endl;
        return;
    }
    if (filename.find_first_of("/") != string::npos) {
        cout << "Error: No such directory" << endl;
        return;
    }
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].isused == 1 && fcbs[i].isHide == 0 && fcbs[i].type == 1 && fcbs[i].name == filename) {
            currentPath = currentPath + filename + "/";
            return;
        }
    }
    cout << "Error: No such directory" << endl;
    return;
}

// cmd �̣߳����ڽ����û�����
void os::run() {
    // �߳� 1���������߳�
    cmd = this_thread::get_id();
    std::cout << "* Welcome to MiniOS 23.5.29 LTS" << std::endl;
    std::cout << "\n"
                 "       �����[���������������[�����������������[ �������������[ ���������������[\n"
                 "       �����U�����X�T�T�T�T�a�^�T�T�����X�T�T�a�����X�T�T�T�����[�����X�T�T�T�T�a\n"
                 "       �����U�����������[     �����U   �����U   �����U���������������[\n"
                 "  ����   �����U�����X�T�T�a     �����U   �����U   �����U�^�T�T�T�T�����U\n"
                 "  �^�����������X�a���������������[   �����U   �^�������������X�a���������������U\n"
                 "   �^�T�T�T�T�a �^�T�T�T�T�T�T�a   �^�T�a    �^�T�T�T�T�T�a �^�T�T�T�T�T�T�a\n"
                 "                                            " << std::endl;
    cout << "* Type 'help' to get help." << endl << endl;

    string command;
    while (true) {
        // ����¼
        if (nowUser == -1) {
            userLogin();
        } else {
            if (currentPath == "czh") {
                cout << "czh@MiniOS:/czh/:";
            } else {
                cout << "czh@MiniOS::/czh" << currentPath << ":";
            }
            cin >> command;
            if (command == "help") {
                cout << "* help: ��ȡ����" << endl
                     << "* hello: ��ӡ Hello World!" << endl
                     << "* exit: �˳�ϵͳ" << endl;
            } else if (command == "exit") {
                cout << "Bye!" << endl;
                system("pause");
                exit(0);
            } else if (command == "print") {
                // ��ȡ hello ����Ĳ��������ܴ��ո�
                string arg;
                getline(cin, arg);
                // ���ݲ����� cmd_handler �߳�
                argument = arg;
                unique_lock<mutex> lock(m); // ��������ֹ����߳�ͬʱ����
                message = 1;
                ready = true;
                cv.notify_all();    // ���������߳�
                cv.wait(lock, [this] { return !ready; });   // �ȴ� ready ��Ϊ false
            } else if (command == "create") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 2;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else if (command == "delete") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 3;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else if (command == "ls") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 4;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else if (command == "cd") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 5;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else if (command == "mkdir") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 6;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else if (command == "register") {
                string arg;
                getline(cin, arg);
                argument = arg;
                // ���Я������������ʾ���Ϸ�
                if (arg.find_first_not_of(" ") != string::npos) {
                    cout << "Error: Invalid argument" << endl;
                    continue;
                }
                unique_lock<mutex> lock(m);
                message = 7;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else if (command == "open") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 8;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            } else {
                cout << command << ": command not found" << endl;
            }
        }
    }
}


// kernel���ں˴������
[[noreturn]] void os::kernel() {
    kernels = this_thread::get_id();
    // ѭ��������� message �仯
    while (true) {
        unique_lock<mutex> lock(m); // ��������ֹ����߳�ͬʱ����
        cv.wait(lock, [this] { return ready; });    // �ȴ� ready ��Ϊ true
        // *1 print ����
        if (message == 1) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            } else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                cout << argument << endl;
            }
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
            // *2 create ����
        else if (message == 2) {
            // ������������ filename �� type
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            } else if (argument.find(' ') == string::npos) {
                cout << "argument is not enough!" << endl;
            } else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // ��ȡ filename
                string filename = argument.substr(0, argument.find(' '));
                // ��ȡ type
                string type = argument.substr(argument.find(' ') + 1);
                // type ֻ��Ϊ dir �� file
                if (type != "dir" && type != "file") {
                    cout << "type is wrong!" << endl;
                } else {
                    // �����ļ�
                    createFile(filename, type == "dir" ? 0 : 1);
                }
            }
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
            // *3 delete ����
        else if (message == 3) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            } else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // ɾ���ļ�
                deleteFile(argument);
            }
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
            // *4 ls ����
        else if (message == 4) {
            // ��ʾ�ļ��б�
            showFileList();

            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        } else if (message == 5) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            } else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // �л�Ŀ¼
                cd(argument);
            }
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
            // *7 register ����
        else if (message == 7) {
            // ע���û�
            userRegister();
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
    }
}


