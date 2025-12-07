
// Expense Splitter with SQLite Database (Single File)
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <sstream>
#include <sqlite3.h>

using namespace std;

class ExpenseManager {
private:
    set<string> users;
    // balances[debtor][creditor] = amount debtor owes creditor
    map<string, map<string,double>> balances;
    sqlite3* db = nullptr;

    bool openDatabase(const string& filename) {
        if (sqlite3_open(filename.c_str(), &db) != SQLITE_OK) {
            cout << "Error opening database: " << sqlite3_errmsg(db) << "\n";
            return false;
        }
        return true;
    }

    bool execSimple(const string& sql) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            cout << "SQL error: " << (errMsg ? errMsg : "unknown") << "\n";
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    static int usersCallback(void* data, int argc, char** argv, char**) {
        auto* self = static_cast<ExpenseManager*>(data);
        if (argc > 0 && argv[0]) {
            string name = argv[0];
            self->users.insert(name);
            self->balances[name]; // ensure entry
        }
        return 0;
    }

    static int balancesCallback(void* data, int argc, char** argv, char**) {
        auto* self = static_cast<ExpenseManager*>(data);
        if (argc >= 3 && argv[0] && argv[1] && argv[2]) {
            string debtor   = argv[0];
            string creditor = argv[1];
            double amount   = stod(argv[2]);
            if (amount > 0) {
                self->balances[debtor][creditor] = amount;
            }
        }
        return 0;
    }

    void loadFromDB() {
        // create tables if not exists
        string createUsers =
            "CREATE TABLE IF NOT EXISTS users ("
            "name TEXT PRIMARY KEY);";

        string createBalances =
            "CREATE TABLE IF NOT EXISTS balances ("
            "debtor TEXT,"
            "creditor TEXT,"
            "amount REAL,"
            "PRIMARY KEY (debtor, creditor));";

        execSimple(createUsers);
        execSimple(createBalances);

        // load users
        char* errMsg = nullptr;
        string sqlUsers = "SELECT name FROM users;";
        int rc = sqlite3_exec(db, sqlUsers.c_str(), usersCallback, this, &errMsg);
        if (rc != SQLITE_OK) {
            cout << "Error loading users: " << (errMsg ? errMsg : "unknown") << "\n";
            sqlite3_free(errMsg);
        }

        // load balances
        string sqlBal = "SELECT debtor, creditor, amount FROM balances;";
        rc = sqlite3_exec(db, sqlBal.c_str(), balancesCallback, this, &errMsg);
        if (rc != SQLITE_OK) {
            cout << "Error loading balances: " << (errMsg ? errMsg : "unknown") << "\n";
            sqlite3_free(errMsg);
        }
    }

    void saveUserToDB(const string& name) {
        string sql = "INSERT OR IGNORE INTO users (name) VALUES ('" + name + "');";
        execSimple(sql);
    }

    void saveAllBalancesToDB() {
        // Clear table and write from memory (simple approach)
        execSimple("DELETE FROM balances;");
        for (auto &debtorPair : balances) {
            const string &debtor = debtorPair.first;
            for (auto &credPair : debtorPair.second) {
                const string &creditor = credPair.first;
                double amount = credPair.second;
                if (amount > 0) {
                    stringstream ss;
                    ss << "INSERT INTO balances (debtor, creditor, amount) VALUES ('"
                       << debtor << "', '" << creditor << "', " << amount
                       << ") ON CONFLICT(debtor, creditor) DO UPDATE SET amount=" << amount << ";";
                    execSimple(ss.str());
                }
            }
        }
    }

public:
    ExpenseManager() {
        if (openDatabase("expenses.db")) {
            loadFromDB();
        } else {
            cout << "Warning: could not open SQLite DB. Running without persistence.\n";
        }
    }

    ~ExpenseManager() {
        if (db) sqlite3_close(db);
    }

    void addUser(const string &name) {
        users.insert(name);
        balances[name]; // ensure entry exists
        cout << "User added: " << name << "\n";

        if (db) {
            saveUserToDB(name);
        }
    }

    bool userExists(const string &name) {
        return users.count(name) > 0;
    }

    void showUsers() {
        cout << "\n--- Users ---\n";
        if (users.empty()) {
            cout << "No users added yet.\n";
            return;
        }
        for (const auto &u : users)
            cout << "- " << u << "\n";
    }

    void addExpense(const string &payer, double amount, const vector<string> &shared) {
        if (!userExists(payer)) {
            cout << "Error: Payer not registered.\n";
            return;
        }

        vector<string> validUsers;
        for (const auto &u : shared) {
            if (userExists(u)) {
                validUsers.push_back(u);
            } else {
                cout << "Skipping unknown user: " << u << "\n";
            }
        }

        if (validUsers.size() == 0) {
            cout << "Error: No valid users to split expense.\n";
            return;
        }

        double split = amount / validUsers.size();

        for (const auto &u : validUsers) {
            if (u != payer) {
                // u owes payer
                balances[u][payer] += split;
                balances[payer][u] -= split;
            }
        }
        cout << "Expense added.\n";

        if (db) {
            saveAllBalancesToDB();
        }
    }

    void settle(const string &from, const string &to, double amount) {
        if (!userExists(from) || !userExists(to)) {
            cout << "Error: Both users must be registered.\n";
            return;
        }

        double owed = balances[from][to];

        if (owed <= 0) {
            cout << "Invalid: " << from << " does not owe " << to << ".\n";
            return;
        }

        if (amount > owed) {
            cout << "Invalid: Cannot settle more than owed (owed ₹" << owed << ").\n";
            return;
        }

        balances[from][to] -= amount;
        balances[to][from] += amount;

        cout << "Payment settled.\n";

        if (db) {
            saveAllBalancesToDB();
        }
    }

    void showBalances() {
        cout << "\n---- Net Balances ----\n";
        bool empty = true;

        for (auto &debtorPair : balances) {
            const string &debtor = debtorPair.first;
            for (auto &credPair : debtorPair.second) {
                const string &creditor = credPair.first;
                double amount = credPair.second;
                if (amount > 0) {
                    cout << debtor << " owes " << creditor << ": ₹" << amount << "\n";
                    empty = false;
                }
            }
        }

        if (empty) cout << "No pending balances.\n";
    }
};

int main() {
    ExpenseManager m;
    int c;

    while (true) {
        cout << "\n===== EXPENSE SPLITTER (SQL + C++) =====\n";
        cout << "1. Add User\n";
        cout << "2. Show Users\n";
        cout << "3. Add Expense\n";
        cout << "4. Settle Payment\n";
        cout << "5. Show Balances\n";
        cout << "6. Exit\n";
        cout << "Choice: ";
        cin >> c;

        if (c == 1) {
            string n;
            cout << "Enter user: ";
            cin >> n;
            m.addUser(n);
        }
        else if (c == 2) {
            m.showUsers();
        }
        else if (c == 3) {
            string payer;
            double amount;
            int count;

            cout << "Payer: ";
            cin >> payer;

            cout << "Amount: ";
            cin >> amount;

            cout << "How many users shared? ";
            cin >> count;

            vector<string> s(count);
            cout << "Enter names:\n";
            for (int i = 0; i < count; i++) cin >> s[i];

            m.addExpense(payer, amount, s);
        }
        else if (c == 4) {
            string f, t;
            double a;

            cout << "From: ";
            cin >> f;

            cout << "To: ";
            cin >> t;

            cout << "Amount: ";
            cin >> a;

            m.settle(f, t, a);
        }
        else if (c == 5) {
            m.showBalances();
        }
        else if (c == 6) {
            break;
        }
        else {
            cout << "Invalid choice.\n";
        }
    }

    return 0;
}
