#include "notifications.hpp"

#include "../notification.hpp"
#include "../notification_json.hpp"

#include <chrono>
#include <vector>

namespace {

// Hardcoded sample data — replaced by a DB query in Phase 1.
// using namespace std::chrono_literals brings in the hour (h) and day (d)
// suffixes so we can write natural durations like 2h instead of
// std::chrono::hours{2}.
std::vector<Notification> sample_notifications()
{
    using namespace std::chrono_literals;
    const auto now = std::chrono::system_clock::now();

    return {
        {1, "Reunião às 15h",       "Não se esqueça da reunião de equipe.", now - 2h},
        {2, "Lembrete de pagamento", "Fatura vence amanhã.",                 now - 24h},
        {3, "Novo comentário",       "Alguém comentou na sua tarefa.",       now - 48h},
    };
}

} // namespace

crow::response handle_get_notifications()
{
    const auto notifications = sample_notifications();

    crow::response res;
    res.code = 200;
    // Without this header, Crow defaults to text/plain.
    // Clients (browsers, curl --json, fetch()) use Content-Type to decide
    // how to parse the body — we must be explicit.
    res.set_header("Content-Type", "application/json");
    // .dump() converts the nlohmann::json value to a UTF-8 string.
    // No argument = compact (no extra whitespace). Pass an int for indentation:
    // .dump(2) gives pretty-printed output — useful when debugging by hand.
    res.body = serialize_notifications(notifications).dump();
    return res;
}
