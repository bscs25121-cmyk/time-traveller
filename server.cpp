// ======================= TIME-TRAVEL DEBUGGER - SERVER TEMPLATE =======================

// Pipeline this file implements, top to bottom:
//   0. Receive  -- stream the client's .trace bytes straight to source.bin on disk
//   1. Pass 0X0   -- validity check (FUNC/FUNC_END matching)
//   2. Pass 0X1   -- resolve(): copy EVERY source line into resolve.bin as [offset][size][string], then patch CALL targets.
//   3. Pass 0X2   -- execute resolve.bin: tokenize ONE line at a time, update the call stack, take a snapshot -> Timeline
//   4. Pass 0X3   -- serialize Timeline -> session.tdbg(header + snapshot records + dense index)
#include <iostream>
#include<sstream>
#include <string>
#include <cstdint>
#include <fstream>
#include <cstdint>
#include <cstdio>
using namespace std;

// ---- Constants ----
const int32_t MAX_VARS_PER_FRAME = 16;
const int32_t MAX_STACK_DEPTH = 64;
const int32_t MAX_FUNCS = 128;
const int32_t MAX_TOKENS = MAX_VARS_PER_FRAME + 2; // kW + func_name + upto 16 params/args
const int32_t MAX_PATCHES = MAX_FUNCS * 4;
const uint64_t MAX_SOURCE_BYTES = 15ULL * 1024 * 1024; // sanity cap on the declared file length
const int32_t IO_BUFFER_SIZE = 64 * 1024;                  // fixed buffer for streaming to/from disk
const int32_t SOCKET_TIMEOUT_SEC = 5;                      // TODO: apply as SO_RCVTIMEO so a deadclient can't hang the server forever

// ---- Custom data structures

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
    // Implement these functions:
    Stack()
    { // initialize the stack
        top = nullptr;
        count = 0;

    }
    void push(const T& val)
    {
        Node* newnode = new newnode{ val,top };
        top = newnode;
        count++;
        // pushes the value on the stack if max limit is not reached yet.
    }
    T pop()
    {
        if (isEmpty)
        {
            cout << "it is already empty";
            break;
        }
        Node* temp = top;
        top = top->next;
        delete temp;

        // pop the top value on the stack
    }
    T& peek()
    {
        return top;// returns the top value on the stack
    }
    bool isEmpty()
    {
        if (count == 0)
        {
            return true;
        }
        else false;
    }
    int32_t depth()
    {
        return count;
    }
    int32_t snapshot_into(T out[], int32_t maxLen)
    {
        int32_t write = 0;

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
// Timeline : doubly linked list of Snapshots
struct Snapshot; // fwd declaration;
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
    // Implement these functions
    Timeline()
    {
        head = nullptr;
        tail = nullptr;
        stepCount = 0;

    }
    void record(Snapshot* s)
    {
        TimelineNode* node = new TimelineNode{ s,nullptr,tail };// add record in the timeline
    }
    TimelineNode* begin()
    {
        return head;
    }
    int32_t getStepCount()
    {
        return stepCount;
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
    // placeholder for other two data members
}

// resolve.bin - bookkeeping
struct FuncEntry
{
    string funcName;
    int64_t byteOffsetInResolveBin; // where this function's FUNC header record sits
};
struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; // where in resolve.bin to seek back and overwrite
    string targetFuncName;
};

// PASS 0x0: READING source.bin + VALIDITY CHECK
bool readSourceLine(ifstream& in, string& out)
{
    // reads the next nonblank line
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
    // for each func defined there should be exactly one func_end and no nested funcs allowed - 
}
// PASS 0x1: RESOLVE() -> resolve.bin
int64_t writeResolveRecord(FILE* f, int64_t offsetField, const string& text)
{
    // writes one [offset(8B)][size(4B)][string] record at the current file position
    // returns this record's own starting byte position
}
int64_t readResolveRecord(FILE* f, string& outText)
{
    // reads one record at the current position and advances past it, returns the offset field - the raw line text comes back untouched in outText.
}
int64_t resolveProgram(const char* sourcePath, const char* resolveBinPath)
{
    FuncEntry funcArray[MAX_FUNCS];
    int32_t funcCount = 0;
    PendingPatch patches[MAX_PATCHES];
    int32_t patchCount = 0;
    // Every source line becomes one record holding the raw line, as-is.
    // resolve() only PEEKS at the leading word(s) -- enough to spot FUNC
    // (remember its position) and CALL (remember which function it needs
    // and where its offset field sits).
    // Once the whole file is written, every CALL's offset field is patched
    // with its target's position. Patching happens after the full write
    // Returns the byte offset of main's FUNC header record.
    // if there is no main return the error 
}

// PASS 0x2: EXECUTION (tokenization happens here)
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
    Frame mainFrame{ "main", 0, {}, -1, {}, 0 };
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
                currFrame.locals[currFrame.localCount++] = Variable{ tokens[i].text, currFrame.argv[i - 2].value };
        }
        else if (kw == "set")
        {
            if (tokenCount >= 3) setVariableVal(currFrame, tokens[1].text, getVariableVal(currFrame, tokens[2].text));
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
            Frame newFrame{ tokens[1].text, 0, {}, (int32_t)ftell(f), {}, 0 };
            for (int i = 2; i < tokenCount && newFrame.argc < MAX_VARS_PER_FRAME; i++)
                newFrame.argv[newFrame.argc++] = Variable{ tokens[i].text, getVariableVal(currFrame, tokens[i].text) };
            callStack.push(newFrame);
            fseek(f, patchOffset, SEEK_SET);
        }
        else if (kw == "func_end")
        {
            int64_t returnPos = currFrame.returnLine;
            callStack.pop();
            if (returnPos != -1) fseek(f, returnPos, SEEK_SET);
        }

        if (!callStack.isEmpty()) timeline.record(buildSnapshot(callStack));
    }
    fclose(f);
}

// PASS 0x3: SERIALIZE TIMELINE
void writeTdbg(Timeline& timeline, const char* tdbgPath)
{
    FILE* f = fopen(tdbgPath, "wb");
    if (!f) return;

    TTDBHeader header{ "TTDB", 1, timeline.getStepCount(), 0 };
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

// main section
int32_t main()
{

    if (!validateProgram("source.bin"))
    {
        // send an error response instead of a .tdbg file
        return 1;
    }

    int64_t mainOffset = resolveProgram("source.bin", "resolve.bin");

    Timeline timeline;
    executeProgram("resolve.bin", mainOffset, timeline);

    writeTdbg(timeline, "session.tdbg");

    return 0;
}