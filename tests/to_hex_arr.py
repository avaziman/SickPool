def to_hex_arr(str):
    
    res = '{'
    for i in range(0, len(str), 2):
        res += '0x' + str[i:i+2] + ','
    res = res[:-1] + '}'

    return res


# VRSC 1
print(to_hex_arr('a735a8adebc861f966aef4649ef884e218aed32979be66ada1ddd149fef79eab'))

# VRSC 1000000
print(to_hex_arr('baa658081ea730e0e591a29c914133d4370cb76365118ac7077c3a864b9235b7'))
print(to_hex_arr('7a9dee91d6faf67f18bea275db221b5d68f94e655710da1297dd8896d53176b3'))
print(to_hex_arr('e8349824644d9c4b13873a97addb0f4f2ac9946b0d047cdab27d3107068634b7'))
