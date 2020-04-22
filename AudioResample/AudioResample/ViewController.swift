//
//  ViewController.swift
//  AudioResample
//
//  Created by jdapple on 2020/4/22.
//  Copyright © 2020 jdapple. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {

    var isRecord: Bool = false
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.setFrameSize(NSSize(width: 320, height: 240))
        let btn = NSButton(title: "开始录制", target: self, action: #selector(click(sender:)))
        btn.frame = CGRect(x: 110, y: 100, width: 100, height: 40)
        view.addSubview(btn)
        verify()
    }
    
    @objc func click(sender: NSButton) {
        isRecord = !isRecord
        if isRecord {
            sender.title = "结束录制";
            DispatchQueue.global().async {
                audio_record()
            }
        } else {
            sender.title = "开始录制";
            set_record_status(0)
        }
    }

}

