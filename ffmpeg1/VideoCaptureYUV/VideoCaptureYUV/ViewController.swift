//
//  ViewController.swift
//  VideoCaptureYUV
//
//  Created by jdapple on 2020/4/24.
//  Copyright © 2020 jdapple. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {

    var isRecord: Bool = false
    
    override func viewDidLoad() {
        super.viewDidLoad()
        let btn = NSButton(title: "开始录制", target: self, action: #selector(click(sender:)))
        btn.frame = CGRect(x: 110, y: 100, width: 100, height: 40)
        view.addSubview(btn)
    }
    
    override func viewWillAppear() {
        super.viewWillAppear()
        view.window?.isRestorable = false
        view.window?.setContentSize(NSSize(width: 320, height: 240))
    }
    
    @objc func click(sender: NSButton) {
        isRecord = !isRecord
        if isRecord {
            sender.title = "结束录制";
            DispatchQueue.global().async {
                video_record()
            }
        } else {
            sender.title = "开始录制";
            set_record_status(0)
        }
    }


}

