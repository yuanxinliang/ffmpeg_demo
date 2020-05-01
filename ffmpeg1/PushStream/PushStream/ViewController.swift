//
//  ViewController.swift
//  PushStream
//
//  Created by jdapple on 2020/4/30.
//  Copyright © 2020 jdapple. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        let btn = NSButton(title: "开始推流", target: self, action: #selector(click(sender:)))
        btn.frame = CGRect(x: 110, y: 100, width: 100, height: 40)
        view.addSubview(btn)
    }
    
    override func viewWillAppear() {
        super.viewWillAppear()
        view.window?.isRestorable = false
        view.window?.setContentSize(NSSize(width: 320, height: 240))
    }
    
    @objc func click(sender: NSButton) {
        push_stream();
    }
}

