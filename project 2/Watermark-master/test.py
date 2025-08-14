# coding=utf-8

import copy
import cv2
import time
import math
import glob
import numpy as np
import script
import os


def psnr(im1, im2):
    if im1.shape != im2.shape or len(im2.shape) < 2:
        return 0

    di = im2.shape[0] * im2.shape[1]
    if len(im2.shape) == 3:
        di = im2.shape[0] * im2.shape[1] * im2.shape[2]

    diff = np.abs(im1 - im2)
    rmse = np.sum(diff * diff) / di
    print(rmse)
    psnr = 20 * np.log10(255 / rmse)
    return psnr


def rotate_about_center(src, angle, scale=1.):
    w = src.shape[1]
    h = src.shape[0]
    rangle = np.deg2rad(angle)  # angle in radians
    nw = (abs(np.sin(rangle) * h) + abs(np.cos(rangle) * w)) * scale
    nh = (abs(np.cos(rangle) * h) + abs(np.sin(rangle) * w)) * scale
    rot_mat = cv2.getRotationMatrix2D((nw * 0.5, nh * 0.5), angle, scale)
    rot_move = np.dot(rot_mat, np.array([(nw - w) * 0.5, (nh - h) * 0.5, 0]))
    rot_mat[0, 2] += rot_move[0]
    rot_mat[1, 2] += rot_move[1]
    return cv2.warpAffine(src, rot_mat, (int(math.ceil(nw)), int(math.ceil(nh))), flags=cv2.INTER_LANCZOS4)

def attack(fname, type):
    img = cv2.imread(fname)
    if img is None:
        print(f"无法读取图像: {fname}")
        return None

    if type == "ori":
        return img

    if type == "blur":
        kernel = np.ones((5,5),np.float32)/25
        return cv2.filter2D(img,-1,kernel)

    if type=="rotate180":
        return rotate_about_center(img,180)

    if type=="rotate90":
        return rotate_about_center(img,90)

    if type=="chop10":
        w,h = img.shape[:2]
        return img[int(w*0.1):,:]

    if type=="chop5":
        w,h = img.shape[:2]
        return img[int(w*0.05):,:]

    if type=="chop30":
        w,h = img.shape[:2]
        return img[int(w*0.3):,:]

    if type == "gray":
        return cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    if type == "redgray":
        return img[:,:,0]

    if type == "saltnoise":
        for k in range(1000):
            i = int(np.random.random() * img.shape[1])
            j = int(np.random.random() * img.shape[0])
            if img.ndim == 2:
                img[j, i] = 255
            elif img.ndim == 3:
                img[j, i, 0] = 255
                img[j, i, 1] = 255
                img[j, i, 2] = 255
        return img

    if type == "randline":
        cv2.rectangle(img,(384,0),(510,128),(0,255,0),3)
        cv2.rectangle(img,(0,0),(300,128),(255,0,0),3)
        cv2.line(img,(0,0),(511,511),(255,0,0),5)
        cv2.line(img,(0,511),(511,0),(255,0,255),5)
        return img

    if type == "cover":
        cv2.circle(img,(256,256), 63, (0,0,255), -1)
        font=cv2.FONT_HERSHEY_SIMPLEX
        cv2.putText(img,'Just DO it ',(10,500), font, 4,(255,255,0),2)
        return img

    if type == "brighter10":
        # 使用cv2.convertScaleAbs可以自动处理溢出问题
        return cv2.convertScaleAbs(img, alpha=1.1, beta=0)

    if type == "darker10":
        # 使用cv2.convertScaleAbs可以自动处理溢出问题
        return cv2.convertScaleAbs(img, alpha=0.9, beta=0)

    if type == "largersize":
        w,h=img.shape[:2]
        return cv2.resize(img,(int(h*1.5),w))

    if type == "smallersize":
        w,h=img.shape[:2]
        return cv2.resize(img,(int(h*0.5),w))

    return img


attack_list = {
    'ori': '原图',
    'rotate180': '旋转180度',
    'rotate90': '旋转90度',
    'chop5': '剪切掉5%',
    'chop10': '剪切掉10%',
    'chop30': '剪切掉30%',
    'saltnoise': '椒盐噪声',
    'vwm': '增加明水印',
    'randline': '随机画线',
    'cover': '随机遮挡',
    'brighter10': '亮度提高10%',
    'darker10': '亮度降低10%'
}


def test_blindwm(alg='DCT', imgname='ts.jpg', wmname='wm.png', times=1):
    handle = script.dctwm

    if alg == 'DCT':
        handle = script.dctwm
    if alg == 'DWT':
        handle = script.dwtwm

    print('\n##############测试' + alg + '盲提取算法，以及鲁棒性')

    btime = time.time()
    for i in range(times):
        img = cv2.imread('./data/' + imgname)
        wm = cv2.imread('./data/' + wmname, cv2.IMREAD_GRAYSCALE)
        wmd = handle.embed(img, wm)
        outname = './output/' + alg + '_' + imgname

    cv2.imwrite(outname, wmd)
    print('嵌入完成，文件保存在 :{},平均耗时 ：{} 毫秒 ,psnr : {}'.format(outname,
                                                                        int((time.time() - btime) * 1000 / times),
                                                                        psnr(img, wmd)))

    # 确保输出目录存在
    os.makedirs('./output/attack/', exist_ok=True)

    for k, v in attack_list.items():
        wmd = attack(outname, k)
        cv2.imwrite('./output/attack/' + k + '_' + imgname, wmd)
        btime = time.time()
        wm = cv2.imread('./data/' + wmname, cv2.IMREAD_GRAYSCALE)
        sim = handle.extract(wmd, wm)
        print('{:10} : 提取水印 {}，提取信息相似度是：{} ,耗时：{} 毫秒.'.format(v, '成功' if sim > 0.7 else '失败', sim,
                                                                              int((time.time() - btime) * 1000)))


def test_report():
    # 确保目录存在
    os.makedirs('./output/test/', exist_ok=True)
    os.makedirs('./output/attack/', exist_ok=True)

    # 第一部分：测试图片
    probsum = 0
    maxsim = 0
    num = 0
    test_files = glob.glob('./output/test/*')

    if test_files:
        for name in test_files:
            wmd = cv2.imread(name)
            wm = cv2.imread('./data/wm.png', cv2.IMREAD_GRAYSCALE)
            if wmd is not None and wm is not None:
                sim = script.dctwm.extract(wmd, wm)
                probsum += sim
                maxsim = max(maxsim, sim)
                num += 1
                print('{} has wm prob: {}'.format(name, sim))

        if num > 0:
            print('avg prob {}, max prob {}'.format(probsum / num, maxsim))
        else:
            print('No valid test images found')
    else:
        print('No test images found in ./output/test/')

    # 第二部分：攻击后的图片
    probsum = 0
    minsim = 1.0
    num = 0
    attack_files = glob.glob('./output/attack/*')

    if attack_files:
        for name in attack_files:
            wmd = cv2.imread(name)
            wm = cv2.imread('./data/wm.png', cv2.IMREAD_GRAYSCALE)
            if wmd is not None and wm is not None:
                sim = script.dctwm.extract(wmd, wm)
                probsum += sim
                minsim = min(minsim, sim)
                num += 1
                print('{} has wm prob: {}'.format(name, sim))

        if num > 0:
            print('avg prob {}, min prob {}'.format(probsum / num, minsim))
        else:
            print('No valid attack images found')
    else:
        print('No attack images found in ./output/attack/')


if __name__ == '__main__':
    # 确保输出目录存在
    os.makedirs('./output', exist_ok=True)
    os.makedirs('./output/test', exist_ok=True)
    os.makedirs('./output/attack', exist_ok=True)

    test_blindwm('DCT', 'ts.jpg', 'wm.png')
    test_blindwm('DCT', 'lena.jpg', 'wm.png')
    test_blindwm('DCT', 'ts.jpg', 'wm.png')
    test_blindwm('DCT', 'tm.jpg', 'wm.png')
    test_blindwm('DCT', 'ta.png', 'wm.png')
    test_blindwm('DCT', 'tb.jpg', 'wm.png')
    test_blindwm('DCT', 'td.jpg', 'wm.png')
    test_blindwm('DCT', 'ss.jpg', 'wm.png')
    test_blindwm('DCT', 'bm.jpg', 'wm.png')

    test_report()