#!/bin/csh


# 512742~512842
# 512843~513843
# 505430~505530
# 505531~505800
# 530000~530499
# 520000~521584
# 521585~530000
@ StartNum = 521585
@ EndNum = 530000
while($StartNum <= $EndNum)
./CaptureTXComic $StartNum
@ StartNum++
echo $StartNum
end
