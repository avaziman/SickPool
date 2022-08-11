// #ifndef CLIENT_LIST_HPP
// #define CLIENT_LIST_HPP

// //
// // why linked list?
// // because we can save the pointer in epoll and by that achieve O(1) access time
// // much faster than unordered_map additionally compared to std::vector linked
// // list has O(1) remove and insert time
// // why doubly linked? because we want to be able to remove with just a node
// // pointer

// #include <memory>
// #include <mutex>

// #include "stratum_client.hpp"

// struct Node
// {
//     std::unique_ptr<StratumClient> data;
//     std::shared_ptr<Node> next;
//     Node* prev;
// };

// // TODO: lock on iteration

// class ClientList
// {
//    public:
//     std::shared_ptr<Node> head;
//     std::mutex clients_mutex;

//     ~ClientList()
//     {
//         while (head) head = std::move(head->next);
//     }

//     void Erase(Node* ptr)
//     {
//         std::unique_lock lock(clients_mutex);
//         // moving next from this->next to this->prev->next
//         if (ptr->next && ptr->prev)
//         {
//             ptr->next->prev = ptr->prev;
//             ptr->prev->next = std::move(ptr->next);
//         }
//         else if (ptr->next)
//         {
//             ptr->next->prev = nullptr;
//         }
//         else if (ptr->prev)
//         {
//             ptr->prev->next = nullptr;
//         }
//     }

//     Node* Add(std::unique_ptr<StratumClient> ptr)
//     {
//         std::unique_lock lock(clients_mutex);
//         auto new_head = std::make_shared<Node>();

//         new_head->data = std::move(ptr);
//         new_head->prev = head.get();
//         if (head)
//         {
//             head->next = new_head;
//         }
//         head = std::move(new_head);
//         return head.get();
//     }
// };


// #endif