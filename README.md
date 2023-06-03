
### Smart Config ##
1. Đầu tiên, vừa vào sẽ khởi tạo smartconfig để bắt wifi.
2. Bắt được wifi, sẽ bắt đầu khởi tạo MQTT client, MQTT handler
    và Task để đọc Trigger_Task, Smartconfig_Task từ BUTTON.

### MQTT handler ### 
1. Khi kết nối được với MQTT handler sẽ bắt đầu tạo
    a. Heartbeat Task để publish mỗi 60 giây {"heartbeat":1}
    b. Trigger Task để nhận flag khi nhấn nút 2 lần và publish trigger đèn

Ở đây sử dụng QOS = 1
    Khi publish thành công sẽ có EVENT_PUBLISH trả về các flag EVENT_TRIG, EVENT_HEART
    Khi publish thì phải đợi tối đa 20 giây bằng xEventGroupWaitBits() để nhận flag EVENT
    Nếu nhận được ACK từ server thì set flag EVENT mới và bắt đầu chu kỳ tiếp theo --> thành công
    Nếu quá 20 giây thì phải gửi lại.

    Cả 2 hàm Trigger() và Heartbeat() đều sử dụng cơ chế đợi EVENT_FLAG từ MQTT_handler->EVENT_PUBLISH

### 4 lần nhấn ###
Thì thay vì smartconfig sẽ restart() lại chương trình
1. Để tránh lỗi
2. Vì chương trình sẽ bắt đầu lại từ SMARTCONFIG init nên vẫn thỏa mãn yêu cầu đề bài.