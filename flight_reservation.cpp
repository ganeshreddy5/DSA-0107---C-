// ============================================================================
//  AeroReserve - Intelligent Flight Reservation and Management System (C++)
//  OOP concepts : classes, encapsulation, vectors, file handling, validation
//  Compile      : g++ flight_reservation.cpp -o aeroreserve   (works with C++11 or newer)
//  Run          : ./aeroreserve        (creates flights.txt and bookings.txt)
// ============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <regex>
#include <ctime>
#include <cctype>
#include <functional>
#include <cstdio>
#include <cstdlib>
using namespace std;

// ---------- rules (same as the web dashboard) ----------
static const char* ID_TYPES[5] = {"Aadhaar", "PAN", "Passport", "Driving Licence", "Voter ID"};
static const char* ID_RE[5]    = {"^[0-9]{12}$", "^[A-Z]{5}[0-9]{4}[A-Z]$", "^[A-Z][0-9]{7}$", "^[A-Z]{2}[0-9]{13}$", "^[A-Z]{3}[0-9]{7}$"};
static const char* ID_HINT[5]  = {"12 digits", "e.g. ABCDE1234F", "e.g. A1234567", "e.g. TN0720110012345", "e.g. ABC1234567"};
const int    MIN_AGE = 11, MAX_AGE = 120;          // passenger must be ABOVE 10 years
const int    ADULT_AGE = 18, BUSINESS_ROWS = 5;    // rows 1-5 Business, 6+ Economy
const double GST = 0.18;

// ---------- small helpers ----------
string clean(const string& s) {                    // strip file delimiters + trim
    string r; for (char c : s) if (c != '|' && c != ';' && c != ',' && c != '\r' && c != '\n') r += c;
    size_t a = r.find_first_not_of(' '); if (a == string::npos) return "";
    return r.substr(a, r.find_last_not_of(' ') - a + 1);
}
string norm(const string& s) { string r; for (char c : s) if (c != ' ' && c != '-') r += (char)toupper((unsigned char)c); return r; }
string lower(string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
string ask(const string& p) { cout << p; string s; if (!getline(cin, s)) exit(0); return clean(s); }
int askInt(const string& p) { string s = ask(p); try { size_t i; int v = stoi(s, &i); return i == s.size() ? v : -1; } catch (...) { return -1; } }
string rs(double v) { return "Rs." + to_string((long)(v + 0.5)); }
string durStr(int m) { return to_string(m / 60) + "h " + to_string(m % 60) + "m"; }
int toMin(const string& t) { return stoi(t.substr(0, 2)) * 60 + stoi(t.substr(3, 2)); }
string hhmm(int m) { m %= 1440; char b[8]; snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60); return b; }
string maskId(const string& s) { return s.size() <= 4 ? s : string(s.size() - 4, '*') + s.substr(s.size() - 4); }
vector<string> split(const string& s, char d) { vector<string> v; string x; stringstream ss(s); while (getline(ss, x, d)) v.push_back(x); return v; }
bool matches(const string& s, const string& re) { return regex_match(s, regex(re)); }
string dayOffset(int n) { time_t t = time(nullptr) + n * 86400; char b[16]; strftime(b, sizeof b, "%Y-%m-%d", localtime(&t)); return b; }
string join(const vector<string>& v) { string r; for (size_t i = 0; i < v.size(); i++) r += (i ? ", " : "") + v[i]; return r; }

// ---------- Passenger ----------
struct Passenger {
    string name; int age = 0; string gender, phone, email, idType, idNo, seat;
    string ser() const { return name + "," + to_string(age) + "," + gender + "," + phone + "," + email + "," + idType + "," + idNo + "," + seat; }
    static Passenger parse(const string& s) {
        auto f = split(s, ','); Passenger p; if (f.size() < 8) return p;
        p.name = f[0]; p.age = stoi(f[1]); p.gender = f[2]; p.phone = f[3]; p.email = f[4]; p.idType = f[5]; p.idNo = f[6]; p.seat = f[7]; return p;
    }
    string category() const { return age < ADULT_AGE ? "Minor" : age >= 60 ? "Senior" : "Adult"; }
};

// ---------- Flight ----------
class Flight {
public:
    string no, airline, from, to, date, dep, arr; int dur = 0, seats = 180; double econ = 0, biz = 0; vector<string> booked;
    int rows() const { return (seats + 5) / 6; }
    static bool parseSeat(const string& s, int& r, int& c) {
        if (s.size() < 2 || s.size() > 4) return false;
        char L = s.back(); if (L < 'A' || L > 'F') return false;
        string n = s.substr(0, s.size() - 1); for (char ch : n) if (!isdigit((unsigned char)ch)) return false;
        r = stoi(n); c = L - 'A'; return r >= 1;
    }
    bool exists(int r, int c) const { return r >= 1 && (r - 1) * 6 + c < seats; }
    bool isBooked(const string& s) const { return find(booked.begin(), booked.end(), s) != booked.end(); }
    void release(const string& s) { booked.erase(remove(booked.begin(), booked.end(), s), booked.end()); }
    static string seatName(int r, int c) { return to_string(r) + string(1, 'A' + c); }
    int available() const { return seats - (int)booked.size(); }
    int occupancy() const { return seats ? (int)(100.0 * booked.size() / seats + 0.5) : 0; }
    string status() const { int o = occupancy(); return o >= 90 ? "CRITICAL" : o >= 75 ? "HIGH" : o >= 50 ? "MEDIUM" : "LOW"; }
    double fare(const string& cls) const { return cls == "Business" ? biz : econ; }
    // Smart recommendation: lower score = better balance of fare, duration and seat availability
    double score(const string& cls) const { return fare(cls) / 1000.0 + dur / 60.0 + (available() < 10 ? 2 : 0); }
    string ser() const {
        return no + "|" + airline + "|" + from + "|" + to + "|" + date + "|" + dep + "|" + arr + "|" + to_string(dur) + "|" + to_string(seats) +
               "|" + to_string((long)econ) + "|" + to_string((long)biz) + "|" + join(booked);
    }
    static bool parse(const string& l, Flight& f) {
        auto v = split(l, '|'); if (v.size() < 11) return false;
        f.no = v[0]; f.airline = v[1]; f.from = v[2]; f.to = v[3]; f.date = v[4]; f.dep = v[5]; f.arr = v[6];
        f.dur = stoi(v[7]); f.seats = stoi(v[8]); f.econ = stod(v[9]); f.biz = stod(v[10]);
        if (v.size() > 11) for (auto& s : split(v[11], ',')) { string t = norm(s); if (!t.empty()) f.booked.push_back(t); }
        return true;
    }
};

// ---------- Booking ----------
class Booking {
public:
    string id, no, airline, from, to, date, dep, arr, cls, status; double total = 0; vector<Passenger> pax;
    vector<string> seats() const { vector<string> v; for (auto& p : pax) v.push_back(p.seat); return v; }
    string ser() const {
        string r = id + "|" + no + "|" + airline + "|" + from + "|" + to + "|" + date + "|" + dep + "|" + arr + "|" + cls + "|" + status + "|" + to_string((long)(total + 0.5)) + "|";
        for (size_t i = 0; i < pax.size(); i++) r += (i ? ";" : "") + pax[i].ser();
        return r;
    }
    static bool parse(const string& l, Booking& b) {
        auto v = split(l, '|'); if (v.size() < 12) return false;
        b.id = v[0]; b.no = v[1]; b.airline = v[2]; b.from = v[3]; b.to = v[4]; b.date = v[5]; b.dep = v[6]; b.arr = v[7];
        b.cls = v[8]; b.status = v[9]; b.total = stod(v[10]);
        for (auto& s : split(v[11], ';')) b.pax.push_back(Passenger::parse(s));
        return true;
    }
};

// ---------- Reservation system ----------
class ReservationSystem {
    vector<Flight> flights; vector<Booking> bookings; int counter = 1001;
    const string FF = "flights.txt", BF = "bookings.txt";

    Flight* findFlight(const string& no) { for (auto& f : flights) if (lower(f.no) == lower(no)) return &f; return nullptr; }
    Booking* findBooking(const string& id) { for (auto& b : bookings) if (lower(b.id) == lower(id)) return &b; return nullptr; }

    // ----- persistence (file handling) -----
    void seed() {
        struct Rt { const char *f, *t; int d, fare; };
        Rt rts[] = {{"Chennai","Delhi",150,4500},{"Delhi","Chennai",155,4700},{"Chennai","Mumbai",120,4000},{"Mumbai","Chennai",125,4100},
                    {"Chennai","Bengaluru",65,2400},{"Hyderabad","Chennai",85,3300},{"Delhi","Mumbai",130,4200},{"Kolkata","Delhi",140,4600}};
        const char* AL[4] = {"Air India", "IndiGo", "SpiceJet", "Vistara"};
        for (int i = 0; i < 8; i++) for (int k = 0; k < 2; k++) {
            Flight f; f.no = "AR" + to_string(990 - i * 3 - k); f.airline = AL[(i + k) % 4]; f.from = rts[i].f; f.to = rts[i].t;
            f.date = dayOffset(1 + k); f.dep = k ? "18:11" : "06:40"; f.dur = rts[i].d + k * 15; f.arr = hhmm(toMin(f.dep) + f.dur);
            f.seats = 180; f.econ = rts[i].fare + k * 300; f.biz = 9000;
            for (int r = BUSINESS_ROWS + 1; r <= f.rows(); r++) for (int c = 0; c < 6; c++)
                if (hash<string>()(f.no + f.date + Flight::seatName(r, c)) % 100 < 7) f.booked.push_back(Flight::seatName(r, c));
            flights.push_back(f);
        }
    }
    void load() {
        string line; ifstream fi(FF);
        while (getline(fi, line)) { Flight f; if (!line.empty() && Flight::parse(line, f)) flights.push_back(f); }
        if (flights.empty()) seed();
        ifstream bi(BF);
        while (getline(bi, line)) { Booking b; if (!line.empty() && Booking::parse(line, b)) bookings.push_back(b); }
        counter = 1001 + (int)bookings.size();
    }
    void save() {
        ofstream fo(FF); for (auto& f : flights) fo << f.ser() << "\n";
        ofstream bo(BF); for (auto& b : bookings) bo << b.ser() << "\n";
    }

    // ----- validation: age + identity proof -----
    string validateGroup(const vector<Passenger>& v) {
        vector<string> seen; bool adult = false;
        for (size_t i = 0; i < v.size(); i++) {
            string key = v[i].idType + norm(v[i].idNo);
            if (find(seen.begin(), seen.end(), key) != seen.end()) return "Passenger " + to_string(i + 1) + ": this ID number is already used by another passenger.";
            seen.push_back(key); if (v[i].age >= ADULT_AGE) adult = true;
        }
        if (!adult) return "At least one adult (18+) must travel in the booking. Minors cannot travel alone.";
        return "";
    }
    Passenger readPassenger(int i) {
        Passenger p; cout << "\n--- Passenger " << i << " (age must be above 10; valid government ID required) ---\n";
        while ((p.name = ask("Full name: ")).empty()) cout << "Name is required.\n";
        for (;;) { p.age = askInt("Age: "); if (p.age >= MIN_AGE && p.age <= MAX_AGE) break; cout << "Passenger must be above 10 years of age (" << MIN_AGE << "-" << MAX_AGE << ").\n"; }
        for (;;) { int g = askInt("Gender (1-Male 2-Female 3-Other): "); if (g >= 1 && g <= 3) { p.gender = g == 1 ? "Male" : g == 2 ? "Female" : "Other"; break; } }
        while (!matches(p.phone = ask("Contact number: "), "^\\+?[0-9]{7,13}$")) cout << "Enter a valid phone number.\n";
        while (!matches(p.email = ask("Email: "), "^[^@ ]+@[^@ ]+\\.[^@ ]+$")) cout << "Enter a valid email address.\n";
        int t; cout << "ID proof type:\n"; for (int k = 0; k < 5; k++) cout << "  " << k + 1 << ". " << ID_TYPES[k] << "\n";
        for (;;) { t = askInt("Choose (1-5): "); if (t >= 1 && t <= 5) break; }
        p.idType = ID_TYPES[t - 1];
        for (;;) { p.idNo = norm(ask(string(ID_TYPES[t - 1]) + " number (" + ID_HINT[t - 1] + "): ")); if (matches(p.idNo, ID_RE[t - 1])) break; cout << "Invalid " << p.idType << " number.\n"; }
        return p;
    }
    vector<Passenger> collectPassengers(int n) {
        for (;;) {
            vector<Passenger> v; for (int i = 1; i <= n; i++) v.push_back(readPassenger(i));
            string e = validateGroup(v); if (e.empty()) return v;
            cout << "\n!! " << e << " Please re-enter the passenger details.\n";
        }
    }

    // ----- seats -----
    bool seatOk(const Flight& f, const string& cls, const string& s, const vector<string>& own) {
        int r, c; if (!Flight::parseSeat(s, r, c) || !f.exists(r, c)) return false;
        if ((cls == "Business") != (r <= BUSINESS_ROWS)) return false;
        return !f.isBooked(s) || find(own.begin(), own.end(), s) != own.end();
    }
    void showMap(const Flight& f, const string& cls, const vector<string>& own) {
        cout << "\n      A B C   D E F      (X occupied, B business free, O economy free, - not in " << cls << ")\n";
        for (int r = 1; r <= f.rows(); r++) {
            cout << setw(3) << r << "   ";
            for (int c = 0; c < 6; c++) {
                string s = Flight::seatName(r, c); char ch;
                if (!f.exists(r, c)) ch = ' '; else if (!seatOk(f, cls, s, own)) ch = (f.isBooked(s) ? 'X' : '-'); else ch = r <= BUSINESS_ROWS ? 'B' : 'O';
                cout << ch << ' '; if (c == 2) cout << "  ";
            }
            cout << "\n";
        }
    }
    vector<string> groupSeats(const Flight& f, const string& cls, int n, const vector<string>& own) {  // group seating optimization
        for (int r = 1; r <= f.rows(); r++) for (int c = 0; c + n <= 6; c++) {
            vector<string> g; for (int k = 0; k < n; k++) g.push_back(Flight::seatName(r, c + k));
            if (all_of(g.begin(), g.end(), [&](const string& s) { return seatOk(f, cls, s, own); })) return g;
        }
        return {};
    }
    vector<string> selectSeats(const Flight& f, const string& cls, int n, const vector<string>& own) {
        showMap(f, cls, own);
        if (n > 1 && lower(ask("Use group seating optimizer? (y/n): ")) == "y") {
            auto g = groupSeats(f, cls, n, own);
            if (!g.empty()) { cout << "Assigned seats: " << join(g) << "\n"; return g; }
            cout << "No row has " << n << " adjacent free seats. Choose manually.\n";
        }
        vector<string> sel;
        while ((int)sel.size() < n) {
            string s = norm(ask("Seat for passenger " + to_string(sel.size() + 1) + " (e.g. " + (cls == "Business" ? "2A" : "7B") + "): "));
            if (!seatOk(f, cls, s, own) || find(sel.begin(), sel.end(), s) != sel.end()) cout << "Invalid, occupied or wrong-class seat!\n"; else sel.push_back(s);
        }
        return sel;
    }

    // ----- output -----
    void ticket(const Booking& b) {
        cout << "\n=========== AEROSERVE BOARDING PASS ===========\nBooking ID : " << b.id << "   [" << b.status << "]\nFlight     : " << b.no << " (" << b.airline << ")\n"
             << "Route      : " << b.from << " -> " << b.to << "\nDate       : " << b.date << "   " << b.dep << " - " << b.arr << "\nClass      : " << b.cls << "\n";
        for (auto& p : b.pax) cout << " * " << p.name << " (" << p.age << ", " << p.gender << ", " << p.category() << ")  " << p.idType << ": " << maskId(p.idNo) << "  Seat " << p.seat << "\n";
        cout << "Total paid : " << rs(b.total) << " (incl. 18% GST)\n===============================================\n";
    }

    // ----- Module 1 + 2: search, recommend, passenger details, seats, booking -----
    void searchAndBook() {
        string from = ask("From: "), to = ask("To: "), date = ask("Date (YYYY-MM-DD): ");
        if (!matches(date, "^[0-9]{4}-[0-9]{2}-[0-9]{2}$") || lower(from) == lower(to) || from.empty()) { cout << "Enter different cities and a valid date.\n"; return; }
        int n = askInt("Passengers (1-6): "); if (n < 1 || n > 6) { cout << "Invalid count.\n"; return; }
        string cls = askInt("Class (1-Economy 2-Business): ") == 2 ? "Business" : "Economy";
        vector<Flight*> L, route;
        for (auto& f : flights) if (lower(f.from) == lower(from) && lower(f.to) == lower(to)) { route.push_back(&f); if (f.date == date) L.push_back(&f); }
        if (L.empty() && !route.empty()) { cout << "No flights on this date - showing other dates.\n"; L = route; }
        if (L.empty()) { cout << "No flights found.\n"; return; }
        Flight* best = *min_element(L.begin(), L.end(), [&](Flight* a, Flight* b) { return a->score(cls) < b->score(cls); });
        int sortBy = askInt("Sort: 1-Best overall  2-Lowest fare  3-Shortest : ");
        sort(L.begin(), L.end(), [&](Flight* a, Flight* b) {
            return sortBy == 2 ? a->fare(cls) < b->fare(cls) : sortBy == 3 ? a->dur < b->dur : a->score(cls) < b->score(cls); });
        cout << "\n" << left << setw(4) << "#" << setw(8) << "Flight" << setw(11) << "Airline" << setw(12) << "Date" << setw(14) << "Dep-Arr" << setw(9) << "Time"
             << setw(10) << "Fare" << setw(7) << "Seats" << "Occupancy\n";
        for (size_t i = 0; i < L.size(); i++)
            cout << setw(4) << i + 1 << setw(8) << L[i]->no << setw(11) << L[i]->airline << setw(12) << L[i]->date << setw(14) << (L[i]->dep + "-" + L[i]->arr)
                 << setw(9) << durStr(L[i]->dur) << setw(10) << rs(L[i]->fare(cls)) << setw(7) << L[i]->available() << L[i]->status() << (L[i] == best ? "  * RECOMMENDED" : "") << "\n";
        int ch = askInt("Select flight # (0 = back): "); if (ch < 1 || ch > (int)L.size()) return;
        Flight& f = *L[ch - 1]; if (f.available() < n) { cout << "Not enough seats.\n"; return; }
        auto pax = collectPassengers(n);
        auto seats = selectSeats(f, cls, n, {}); for (int i = 0; i < n; i++) pax[i].seat = seats[i];
        double base = f.fare(cls) * n;
        cout << "\n====== BOOKING SUMMARY ======\n" << f.no << " " << f.from << " -> " << f.to << "  " << f.date << "  " << cls << "\nSeats: " << join(seats)
             << "\nBase fare (" << n << " x " << rs(f.fare(cls)) << "): " << rs(base) << "\nGST 18%: " << rs(base * GST) << "\nGRAND TOTAL: " << rs(base * (1 + GST)) << "\n";
        if (lower(ask("Confirm booking? (y/n): ")) != "y") { cout << "Booking discarded.\n"; return; }
        Booking b; b.id = "BK" + to_string(time(nullptr) % 1000000) + to_string(counter++); b.no = f.no; b.airline = f.airline; b.from = f.from; b.to = f.to;
        b.date = f.date; b.dep = f.dep; b.arr = f.arr; b.cls = cls; b.status = "CONFIRMED"; b.total = base * (1 + GST); b.pax = pax;
        for (auto& s : seats) f.booked.push_back(s);
        bookings.push_back(b); save(); cout << "\nBooking confirmed!\n"; ticket(b);
    }

    // ----- Module 3: reservation & record management -----
    void listBookings() {
        string k = lower(ask("Search by ID / name / route / status (blank = all): ")); int n = 0;
        cout << left << setw(15) << "Booking ID" << setw(8) << "Flight" << setw(22) << "Route" << setw(12) << "Date" << setw(10) << "Seats" << setw(10) << "Class" << setw(11) << "Status" << "Amount\n";
        for (auto& b : bookings) {
            string hay = lower(b.id + b.no + b.from + b.to + b.status); for (auto& p : b.pax) hay += lower(p.name);
            if (hay.find(k) == string::npos) continue;
            n++;
            cout << setw(15) << b.id << setw(8) << b.no << setw(22) << (b.from + "->" + b.to) << setw(12) << b.date << setw(10) << join(b.seats()) << setw(10) << b.cls << setw(11) << b.status << rs(b.total) << "\n";
        }
        if (!n) cout << "No records found.\n";
    }
    void modify() {
        Booking* b = findBooking(ask("Booking ID: ")); Flight* f = b ? findFlight(b->no) : nullptr;
        if (!b || !f || b->status == "CANCELLED") { cout << "Booking not found or cancelled.\n"; return; }
        auto old = b->seats(); int n = b->pax.size();
        auto pax = collectPassengers(n); auto seats = selectSeats(*f, b->cls, n, old);
        for (auto& s : old) f->release(s);
        for (int i = 0; i < n; i++) { pax[i].seat = seats[i]; f->booked.push_back(seats[i]); }
        b->pax = pax; b->status = "MODIFIED"; save(); cout << "Booking modified.\n"; ticket(*b);
    }
    void cancel() {
        Booking* b = findBooking(ask("Booking ID: ")); if (!b || b->status == "CANCELLED") { cout << "Booking not found or already cancelled.\n"; return; }
        if (lower(ask("Cancel " + b->id + "? (y/n): ")) != "y") return;
        if (Flight* f = findFlight(b->no)) for (auto& s : b->seats()) f->release(s);
        b->status = "CANCELLED"; save(); cout << "Cancelled. Seats released.\n";
    }
    void myBookings() {
        for (;;) {
            cout << "\n--- MY BOOKINGS ---\n1.List/Search  2.View ticket  3.Modify  4.Cancel  0.Back\n";
            int c = askInt("Choice: ");
            if (c == 1) listBookings(); else if (c == 2) { if (Booking* b = findBooking(ask("Booking ID: "))) ticket(*b); else cout << "Not found.\n"; }
            else if (c == 3) modify(); else if (c == 4) cancel(); else if (c == 0) return;
        }
    }

    // ----- Admin: dashboard + flight management -----
    void adminStats() {
        int pass = 0, high = 0, crit = 0, avail = 0;
        for (auto& b : bookings) if (b.status != "CANCELLED") pass += b.pax.size();
        for (auto& f : flights) { avail += f.available(); if (f.status() == "HIGH") high++; if (f.status() == "CRITICAL") crit++; }
        cout << "\n===== ADMIN DASHBOARD =====\nTotal flights: " << flights.size() << " | Reservations: " << bookings.size() << " | Passengers: " << pass
             << " | Available seats: " << avail << " | High occupancy: " << high << " | Critical: " << crit << "\n\n";
        cout << left << setw(8) << "Flight" << setw(11) << "Airline" << setw(22) << "Route" << setw(12) << "Date" << setw(7) << "Dep" << setw(7) << "Seats" << setw(8) << "Booked" << setw(7) << "Avail" << setw(6) << "Occ" << "Status\n";
        for (auto& f : flights)
            cout << setw(8) << f.no << setw(11) << f.airline << setw(22) << (f.from + "->" + f.to) << setw(12) << f.date << setw(7) << f.dep << setw(7) << f.seats
                 << setw(8) << f.booked.size() << setw(7) << f.available() << setw(6) << (to_string(f.occupancy()) + "%") << f.status() << "\n";
    }
    string askDef(const string& p, const string& def) { string s = ask(p + (def.empty() ? "" : " [" + def + "]") + ": "); return s.empty() ? def : s; }
    bool readFlight(Flight& f, const Flight* old) {          // used for both Add and Edit
        f.no = norm(askDef("Flight number", old ? old->no : "")); f.airline = askDef("Airline", old ? old->airline : "");
        f.from = askDef("Source", old ? old->from : ""); f.to = askDef("Destination", old ? old->to : "");
        f.date = askDef("Date (YYYY-MM-DD)", old ? old->date : dayOffset(1)); f.dep = askDef("Departure (HH:MM)", old ? old->dep : "");
        try {
            f.dur = stoi(askDef("Duration (minutes)", old ? to_string(old->dur) : "120")); f.seats = stoi(askDef("Total seats (6-180)", old ? to_string(old->seats) : "180"));
            f.econ = stod(askDef("Economy fare", old ? to_string((long)old->econ) : "4500")); f.biz = stod(askDef("Business fare", old ? to_string((long)old->biz) : "9000"));
        } catch (...) { cout << "Invalid number.\n"; return false; }
        string e;
        if (f.no.empty() || f.airline.empty() || f.from.empty() || f.to.empty()) e = "All fields are required.";
        else if (lower(f.from) == lower(f.to)) e = "Source and destination must differ.";
        else if (!matches(f.date, "^[0-9]{4}-[0-9]{2}-[0-9]{2}$") || !matches(f.dep, "^([01][0-9]|2[0-3]):[0-5][0-9]$")) e = "Invalid date or time.";
        else if (f.dur <= 0 || f.seats < 6 || f.seats > 180 || f.econ <= 0 || f.biz <= 0) e = "Duration, seats (6-180) and fares must be valid.";
        else if (findFlight(f.no) && !(old && lower(f.no) == lower(old->no))) e = "Flight number already exists.";
        else if (old && f.seats < (int)old->booked.size()) e = "Seats cannot be fewer than already booked.";
        if (!e.empty()) { cout << e << "\n"; return false; }
        f.arr = hhmm(toMin(f.dep) + f.dur); if (old) f.booked = old->booked; return true;
    }
    void admin() {
        for (;;) {
            adminStats(); cout << "\n1.Add flight  2.Edit flight  3.Delete flight  0.Back\n"; int c = askInt("Choice: ");
            if (c == 1) { Flight f; if (readFlight(f, nullptr)) { flights.push_back(f); save(); cout << "Flight added.\n"; } }
            else if (c == 2) {
                Flight* o = findFlight(ask("Flight number to edit: ")); if (!o) { cout << "Not found.\n"; continue; }
                Flight f, oldCopy = *o; cout << "(press Enter to keep current value)\n";
                if (readFlight(f, &oldCopy)) {
                    for (auto& b : bookings) if (b.no == oldCopy.no) { b.no = f.no; b.airline = f.airline; b.from = f.from; b.to = f.to; b.date = f.date; b.dep = f.dep; b.arr = f.arr; }
                    *o = f; save(); cout << "Flight updated.\n";
                }
            } else if (c == 3) {
                Flight* o = findFlight(ask("Flight number to delete: ")); if (!o) { cout << "Not found.\n"; continue; }
                if (lower(ask("Delete " + o->no + " and cancel its reservations? (y/n): ")) != "y") continue;
                for (auto& b : bookings) if (b.no == o->no) b.status = "CANCELLED";
                flights.erase(flights.begin() + (o - &flights[0])); save(); cout << "Flight deleted.\n";
            } else if (c == 0) return;
        }
    }
public:
    ReservationSystem() { load(); save(); }
    void run() {
        for (;;) {
            cout << "\n===== AEROSERVE - FLIGHT RESERVATION SYSTEM =====\n1.Search & Book  2.My Bookings  3.Admin  0.Exit\n";
            int c = askInt("Choice: ");
            if (c == 1) searchAndBook(); else if (c == 2) myBookings(); else if (c == 3) admin(); else if (c == 0) return;
        }
    }
};

int main() { ReservationSystem().run(); return 0; }
