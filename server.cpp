// ======================= TIME-TRAVEL DEBUGGER - SERVER TEMPLATE =======================

// Pipeline this file implements, top to bottom:
//   0. Receive  -- stream the client's .trace bytes straight to source.bin on disk
//   1. Pass 0X0   -- validity check (FUNC/FUNC_END matching)
//   2. Pass 0X1   -- resolve(): copy EVERY source line into resolve.bin as [offset][size][string], then patch CALL targets.
//   3. Pass 0X2   -- execute resolve.bin: tokenize ONE line at a time, update the call stack, take a snapshot -> Timeline
//   4. Pass 0X3   -- serialize Timeline -> session.tdbg(header + snapshot records + dense index)

#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <cstring>

using namespace std;

// ---- Constants ----
const int32_t MAX_VARS_PER_FRAME = 16;
const int32_t MAX_STACK_DEPTH = 64;
const int32_t MAX_FUNCS = 128;
const int32_t MAX_TOKENS = MAX_VARS_PER_FRAME + 2; // kW + func_name + upto 16 params/args
const int32_t MAX_PATCHES = MAX_FUNCS * 4;
const uint64_t MAX_SOURCE_BYTES = 15ULL * 1024 * 1024; // sanity cap on the declared file length
const int32_t IO_BUFFER_SIZE = 64 * 1024;                  // fixed buffer for streaming to/from disk
const int32_t SOCKET_TIMEOUT_SEC = 5;                      // TODO: apply as SO_RCVTIMEO so a dead client can't hang the server forever

// ---- Custom data structures ----

// Stack: back the live Call Stack during execution
template <typename T>
class Stack
{
    struct Node
    {
        T data;
        Node* next;
    };
    Node* top;
    int32_t count;

public:
    Stack()
    {
        top = nullptr;
        count = 0;
    }

    ~Stack()
    {
        while (!isEmpty())
        {
            pop();
        }
    }

    void push(const T& val)
    {
        if (count >= MAX_STACK_DEPTH)
        {
            return;
        }
        Node* newNode = new Node{ val, top };
        top = newNode;
        count++;
    }

    T pop()
    {
        if (isEmpty())
        {
            return T();
        }
        Node* temp = top;
        T val = temp->data;
        top = top->next;
        delete temp;
        count--;
        return val;
    }

    T& peek()
    {
        return top->data;
    }

    bool isEmpty() const
    {
        return top == nullptr;
    }

    int32_t depth() const
    {
        return count;
    }

    int32_t snapshot_into(T out[], int32_t maxLen)
    {
        Node* curr = top;
        int32_t written = 0;
        while (curr != nullptr && written < maxLen)
        {
            out[written++] = curr->data;
            curr = curr->next;
        }
        return written;
    }
};

// Core structs
struct Variable
{
    string name;
    int32_t value;
};

struct Frame
{
    string func_name;
    int32_t argc;
    Variable argv[MAX_VARS_PER_FRAME];
    int32_t returnLine;
    Variable locals[MAX_VARS_PER_FRAME];
    int32_t localCount;
};

struct Snapshot
{
    Frame callStack[MAX_STACK_DEPTH];
    int32_t stackDepth;
};

// Timeline : doubly linked list of Snapshots
struct TimelineNode
{
    Snapshot* data;
    TimelineNode* next;
    TimelineNode* prev;
};

class Timeline
{
    TimelineNode* head, * tail;
    int32_t stepCount;

public:
    Timeline()
    {
        head = nullptr;
        tail = nullptr;
        stepCount = 0;
    }

    ~Timeline()
    {
        TimelineNode* curr = head;
        while (curr != nullptr)
        {
            TimelineNode* next = curr->next;
            delete curr->data;
            delete curr;
            curr = next;
        }
    }

    void record(Snapshot* s)
    {
        TimelineNode* node = new TimelineNode{ s, nullptr, tail };
        if (tail != nullptr)
        {
            tail->next = node;
        }
        else
        {
            head = node;
        }
        tail = node;
        stepCount++;
    }

    TimelineNode* begin() const
    {
        return head;
    }

    int32_t getStepCount() const
    {
        return stepCount;
    }
};

struct TTDBHeader
{
    char magic[4]; // "TTDB"
    int32_t version;
    int32_t stepCount;
    int64_t indexOffset;
};

void writeHeader(FILE* f, const TTDBHeader& h)
{
    fseek(f, 0, SEEK_SET);
    fwrite(h.magic, 1, 4, f);
    fwrite(&h.version, sizeof(int32_t), 1, f);
    fwrite(&h.stepCount, sizeof(int32_t), 1, f);
    fwrite(&h.indexOffset, sizeof(int64_t), 1, f);
}

struct FuncEntry
{
    string funcName;
    int64_t byteOffsetInResolveBin; 
};

struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; 
    string targetFuncName;
};

// ---- Helper Functions for String/Variable Processing ----

static string trim(const string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static bool isInteger(const string& s)
{
    if (s.empty()) return false;
    size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (start == s.length()) return false;
    for (size_t i = start; i < s.length(); i++)
    {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

static int32_t getVariableVal(const Frame& frame, const string& token)
{
    if (isInteger(token))
    {
        return stoi(token);
    }
    for (int i = 0; i < frame.localCount; i++)
    {
        if (frame.locals[i].name == token)
            return frame.locals[i].value;
    }
    for (int i = 0; i < frame.argc; i++)
    {
        if (frame.argv[i].name == token)
            return frame.argv[i].value;
    }
    return 0;
}

static void setVariableVal(Frame& frame, const string& varName, int32_t val)
{
    for (int i = 0; i < frame.localCount; i++)
    {
        if (frame.locals[i].name == varName)
        {
            frame.locals[i].value = val;
            return;
        }
    }
    for (int i = 0; i < frame.argc; i++)
    {
        if (frame.argv[i].name == varName)
        {
            frame.argv[i].value = val;
            return;
        }
    }
    if (frame.localCount < MAX_VARS_PER_FRAME)
    {
        frame.locals[frame.localCount++] = Variable{ varName, val };
    }
}

// PASS 0x0: READING source.bin + VALIDITY CHECK
bool readSourceLine(ifstream& in, string& out)
{
    string line;
    while (getline(in, line))
    {
        size_t commentPos = line.find("//");
        if (commentPos != string::npos)
        {
            line = line.substr(0, commentPos);
        }
        line = trim(line);
        if (!line.empty())
        {
            out = line;
            return true;
        }
    }
    return false;
}

string firstWord(const string& line)
{
    const char* p = line.c_str();
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    string word = "";
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
    {
        word += *p;
        p++;
    }
    return word;
}

string secondWord(const string& line)
{
    const char* p = line.c_str();
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    string word = "";
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
    {
        word += *p;
        p++;
    }
    return word;
}

bool validateProgram(const char* sourcePath)
{
    ifstream in(sourcePath);
    if (!in.is_open()) return false;

    string line;
    bool inFunc = false;

    while (readSourceLine(in, line))
    {
        string kw = firstWord(line);
        if (kw == "func")
        {
            if (inFunc)
            {
                in.close();
                return false; // Rejects nested function declarations
            }
            inFunc = true;
        }
        else if (kw == "func_end")
        {
            if (!inFunc)
            {
                in.close();
                return false; 
            }
            inFunc = false;
        }
    }

    in.close();
    return !inFunc;
}

// PASS 0x1: RESOLVE() -> resolve.bin
int64_t writeResolveRecord(FILE* f, int64_t offsetField, const string& text)
{
    int64_t pos = ftell(f);
    int32_t size = text.size();
    fwrite(&offsetField, sizeof(int64_t), 1, f);
    fwrite(&size, sizeof(int32_t), 1, f);
    fwrite(text.c_str(), 1, size, f);
    return pos;
}

int64_t readResolveRecord(FILE* f, string& outText)
{
    int64_t offsetField = 0;
    int32_t size = 0;
    if (fread(&offsetField, sizeof(int64_t), 1, f) != 1) return -1;
    if (fread(&size, sizeof(int32_t), 1, f) != 1) return -1;

    char* buf = new char[size + 1];
    fread(buf, 1, size, f);
    buf[size] = '\0';
    outText = string(buf);
    delete[] buf;

    return offsetField;
}

int64_t resolveProgram(const char* sourcePath, const char* resolveBinPath)
{
    ifstream in(sourcePath);
    if (!in.is_open()) return -1;

    FILE* f = fopen(resolveBinPath, "wb");
    if (!f) return -1;

    FuncEntry funcArray[MAX_FUNCS];
    int32_t funcCount = 0;
    PendingPatch patches[MAX_PATCHES];
    int32_t patchCount = 0;

    int64_t mainOffset = -1;
    string line;

    while (readSourceLine(in, line))
    {
        string kw = firstWord(line);

        if (kw == "func")
        {
            string fname = secondWord(line);
            int64_t recordOffset = ftell(f);
            if (funcCount < MAX_FUNCS)
            {
                funcArray[funcCount++] = FuncEntry{ fname, recordOffset };
            }
            if (fname == "main")
            {
                mainOffset = recordOffset;
            }
            writeResolveRecord(f, 0, line);
        }
        else if (kw == "call")
        {
            string targetFunc = secondWord(line);
            int64_t patchOffset = ftell(f);
            if (patchCount < MAX_PATCHES)
            {
                patches[patchCount++] = PendingPatch{ patchOffset, targetFunc };
            }
            writeResolveRecord(f, 0, line);
        }
        else
        {
            writeResolveRecord(f, 0, line);
        }
    }
    in.close();

    // Patch target jump offsets for calls
    for (int i = 0; i < patchCount; i++)
    {
        int64_t targetOffset = -1;
        for (int j = 0; j < funcCount; j++)
        {
            if (funcArray[j].funcName == patches[i].targetFuncName)
            {
                targetOffset = funcArray[j].byteOffsetInResolveBin;
                break;
            }
        }
        if (targetOffset != -1)
        {
            fseek(f, patches[i].byteOffsetOfOffsetField, SEEK_SET);
            fwrite(&targetOffset, sizeof(int64_t), 1, f);
        }
        else
        {
            fclose(f);
            return -1; 
        }
    }

    fclose(f);
    return mainOffset;
}
enum TokenType
{
    KEYWORD,
    IDENTIFIER,
    PARAM
};
struct Token
{
    TokenType type;
    string text;
};

int32_t tokenizeLine(const string& line, Token tokens[], int32_t maxTokens)
{
    const char* p = line.c_str();
    int32_t count = 0;
    while (*p != '\0' && count < maxTokens)
    {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;
        string word = "";
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
        {
            word += *p;
            p++;
        }
        if (count == 0)
            tokens[count++] = Token{ KEYWORD, word };
        else if (count == 1)
            tokens[count++] = Token{ IDENTIFIER, word };
        else
            tokens[count++] = Token{ PARAM, word };
    }
    return count;
}
Snapshot* buildSnapshot(Stack<Frame>& callStack)
{
    Snapshot* s = new Snapshot();
    s->stackDepth = callStack.snapshot_into(s->callStack, MAX_STACK_DEPTH);
    return s;
}
void executeProgram(const char* resolveBinPath, int64_t mainOffset, Timeline& timeline)
{
    if (mainOffset < 0) return;
    FILE* f = fopen(resolveBinPath, "rb");
    if (!f) return;
    Stack<Frame> callStack;
    Frame mainFrame;
    mainFrame.func_name = "main";
    mainFrame.argc = 0;
    mainFrame.returnLine = -1; 
    mainFrame.localCount = 0;
    callStack.push(mainFrame);
    fseek(f, mainOffset, SEEK_SET);

    Token tokens[MAX_TOKENS];
    string lineText;

    while (!callStack.isEmpty())
    {
        int64_t currentOffset = ftell(f);
        int64_t patchOffset = readResolveRecord(f, lineText);
        if (lineText.empty()) break;

        int32_t tokenCount = tokenizeLine(lineText, tokens, MAX_TOKENS);
        if (tokenCount == 0) continue;

        string kw = tokens[0].text;
        Frame& currFrame = callStack.peek();

        if (kw == "func")
        {
            for (int i = 2; i < tokenCount && currFrame.localCount < MAX_VARS_PER_FRAME; i++)
            {
                currFrame.locals[currFrame.localCount++] = Variable{ tokens[i].text, currFrame.argv[i - 2].value };
            }
        }
        else if (kw == "set")
        {
            if (tokenCount >= 3)
            {
                int32_t val = getVariableVal(currFrame, tokens[2].text);
                setVariableVal(currFrame, tokens[1].text, val);
            }
        }
        else if (kw == "add" || kw == "sub" || kw == "mul" || kw == "div")
        {
            if (tokenCount >= 3)
            {
                int32_t val1 = getVariableVal(currFrame, tokens[1].text);
                int32_t val2 = getVariableVal(currFrame, tokens[2].text);
                int32_t res = 0;
                if (kw == "add") res = val1 + val2;
                else if (kw == "sub") res = val1 - val2;
                else if (kw == "mul") res = val1 * val2;
                else if (kw == "div") res = (val2 != 0) ? (val1 / val2) : 0;

                setVariableVal(currFrame, tokens[1].text, res);
            }
        }
        else if (kw == "call")
        {
            Frame newFrame;
            newFrame.func_name = tokens[1].text;
            newFrame.argc = 0;
            newFrame.localCount = 0;
            newFrame.returnLine = ftell(f); 
            for (int i = 2; i < tokenCount && newFrame.argc < MAX_VARS_PER_FRAME; i++)
            {
                int32_t argVal = getVariableVal(currFrame, tokens[i].text);
                newFrame.argv[newFrame.argc++] = Variable{ tokens[i].text, argVal };
            }
            callStack.push(newFrame);
            fseek(f, patchOffset, SEEK_SET); 
        }
        else if (kw == "func_end")
        {
            int64_t returnPos = currFrame.returnLine;
            callStack.pop();
            if (returnPos != -1)
            {
                fseek(f, returnPos, SEEK_SET);
            }
        }
        if (!callStack.isEmpty())
        {
            timeline.record(buildSnapshot(callStack));
        }
    }
    fclose(f);
}
// PASS 0x3: SERIALIZE TIMELINE
void writeTdbg(Timeline& timeline, const char* tdbgPath)
{
    FILE* f = fopen(tdbgPath, "wb");
    if (!f) return;
    TTDBHeader header;
    memcpy(header.magic, "TTDB", 4);
    header.version = 1;
    header.stepCount = timeline.getStepCount();
    header.indexOffset = 0;
    fwrite(&header, sizeof(TTDBHeader), 1, f);
    int32_t stepCount = timeline.getStepCount();
    int64_t* indexArray = new int64_t[stepCount];
    TimelineNode* curr = timeline.begin();
    int32_t idx = 0;
    while (curr != nullptr)
    {
        indexArray[idx++] = ftell(f);
        Snapshot* s = curr->data;
        fwrite(&s->stackDepth, sizeof(int32_t), 1, f);
        for (int i = 0; i < s->stackDepth; i++)
        {
            Frame& fr = s->callStack[i];
            int32_t nameLen = fr.func_name.size();
            fwrite(&nameLen, sizeof(int32_t), 1, f);
            fwrite(fr.func_name.c_str(), 1, nameLen, f);
            fwrite(&fr.returnLine, sizeof(int32_t), 1, f);
            fwrite(&fr.argc, sizeof(int32_t), 1, f);
            for (int a = 0; a < fr.argc; a++)
            {
                int32_t vNameLen = fr.argv[a].name.size();
                fwrite(&vNameLen, sizeof(int32_t), 1, f);
                fwrite(fr.argv[a].name.c_str(), 1, vNameLen, f);
                fwrite(&fr.argv[a].value, sizeof(int32_t), 1, f);
            }
            fwrite(&fr.localCount, sizeof(int32_t), 1, f);
            for (int l = 0; l < fr.localCount; l++)
            {
                int32_t vNameLen = fr.locals[l].name.size();
                fwrite(&vNameLen, sizeof(int32_t), 1, f);
                fwrite(fr.locals[l].name.c_str(), 1, vNameLen, f);
                fwrite(&fr.locals[l].value, sizeof(int32_t), 1, f);
            }
        }
        curr = curr->next;
    }
    header.indexOffset = ftell(f);
    fwrite(indexArray, sizeof(int64_t), stepCount, f);
    writeHeader(f, header);
    delete[] indexArray;
    fclose(f);
}
int32_t main()
{
    if (!validateProgram("source.bin"))
    {
        cerr << "Error: Source program validation failed." << endl;
        return 1;
    }

    int64_t mainOffset = resolveProgram("source.bin", "resolve.bin");
    if (mainOffset < 0)
    {
        cerr << "Error: main function not found or unresolved call." << endl;
        return 1;
    }
    Timeline timeline;
    executeProgram("resolve.bin", mainOffset, timeline);
    writeTdbg(timeline, "session.tdbg");
    return 0;
}