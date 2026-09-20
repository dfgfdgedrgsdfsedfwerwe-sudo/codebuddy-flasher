要从 `struct tm` 结构体中获取星期几的信息，可以使用 `tm_wday` 成员。`tm_wday` 是一个整数，表示星期几，范围从 0 到 6，其中：

- 0 表示星期日
- 1 表示星期一
- 2 表示星期二
- 3 表示星期三
- 4 表示星期四
- 5 表示星期五
- 6 表示星期六

你可以通过 `tm_wday` 来获取星期几的信息，并将其转换为字符串表示。

以下是一个示例代码，展示如何获取并打印星期几的信息：

```cpp
#include <iostream>
#include <ctime>

void TimerModel::printLocalTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("No time available (yet)");
        return;
    }

    // 星期几的字符串表示
    const char* daysOfWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    // 获取星期几的信息
    int dayOfWeek = timeinfo.tm_wday;

    // 打印星期几
    Serial.print("Day of the week: ");
    Serial.println(daysOfWeek[dayOfWeek]);
}
```

### 解释：
- `timeinfo.tm_wday` 返回一个整数，表示当前日期是星期几。
- `daysOfWeek` 是一个字符串数组，包含了星期几的名称。
- 通过 `daysOfWeek[dayOfWeek]`，你可以将 `tm_wday` 的值转换为对应的星期几字符串。

例如，如果 `tm_wday` 的值是 1，那么 `daysOfWeek[1]` 将返回 `"Monday"`。

### 输出示例：
如果当前日期是星期三，输出将是：
```
Day of the week: Wednesday
```

你可以根据需要进一步处理或显示星期几的信息。


要将 `struct tm` 变量转换为字符串变量，可以使用 `strftime` 函数。`strftime` 函数允许你根据指定的格式将 `struct tm` 结构体中的时间信息格式化为字符串。

以下是一个示例代码，展示了如何将 `struct tm` 变量转换为字符串变量：

```cpp
#include <iostream>
#include <ctime>
#include <cstring>

void TimerModel::printLocalTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("No time available (yet)");
        return;
    }

    char buffer[80];  // 用于存储格式化后的时间字符串
    strftime(buffer, sizeof(buffer), "%A, %B %d %Y %H:%M:%S", &timeinfo);

    Serial.println(buffer);  // 输出格式化后的时间字符串
}
```

### 解释：
- `strftime` 函数的第一个参数是一个字符数组（`buffer`），用于存储格式化后的字符串。
- 第二个参数是 `buffer` 的大小，确保不会发生缓冲区溢出。
- 第三个参数是格式化字符串，指定了输出的时间格式。
- 第四个参数是指向 `struct tm` 结构体的指针，包含了要格式化的时间信息。

在这个例子中，`buffer` 数组将包含格式化后的时间字符串，例如 `"Monday, October 30 2023 14:45:30"`，然后通过 `Serial.println(buffer)` 输出到串口。

你可以根据需要调整格式化字符串 `"%A, %B %d %Y %H:%M:%S"` 来输出不同的时间格式。