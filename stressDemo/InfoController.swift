//
//  InfoController.swift
//  stressDemo
//
//  Created by Prasenjit on 16/8/2021.
//


import UIKit
import Charts

class InfoController: UIViewController, BleSingletonDelegate {
    
    @IBOutlet weak var hr_field: UITextField!
    @IBOutlet weak var temp_field: UITextField!
    @IBOutlet weak var resp_field: UITextField!
    @IBOutlet weak var disconnect_button: UIButton!
    @IBOutlet weak var hrLineChartView: LineChartView!
    @IBOutlet weak var stress_field: UITextField!
    var hrDataEntries = [ChartDataEntry]()
    var hrXvalue = 0.0
    
    @IBOutlet weak var respLineChartView: LineChartView!
    var respDataEntries = [ChartDataEntry]()
    var respXvalue = 0.0
    

    
    var temp_disp: Double?
    var temp_in_fahrenheit: Bool?
    var temp_in_fah_ctrl_state: Bool = false // state of temp unit
    
    var userAge: Int = 23 //default age
    
    @IBAction func disconnect(_ sender: UIButton) {
        BleSingleton.shared.disconnect()
        _ = self.navigationController?.popToRootViewController(animated: true)
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        UIApplication.shared.isIdleTimerDisabled = true
        
        if BleSingleton.shared.numDevicesFound <= 0 {
            let alert = UIAlertController(title: "No Devices Found", message: "No HLT devices were found nearby. Please search again.", preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "OK", style: .default, handler:  { action in
                    self.disconnect(self.disconnect_button)
                }))
            self.present(alert, animated: true, completion: nil)
        }
        
        hrLineChartView = setupChartView(hrLineChartView)
        respLineChartView = setupChartView(respLineChartView)
        
    }
    override func viewDidAppear(_ animated: Bool) {
        let alert = UIAlertController(title: "Enter Age", message: "How old are you? (in years)", preferredStyle: .alert)
        self.present(alert, animated: true, completion: nil)
        
        alert.addTextField{ (textField) in
            textField.keyboardType = .numberPad     //show number kaypad
            textField.placeholder = "23" //default placeholder
        }
        
        alert.addAction(UIAlertAction(title: "Submit", style: .default, handler: {[self, weak alert] (_) in
            guard let textField = alert?.textFields?[0], let input = textField.text
            else {
                print("No input provided for age")
                return
            }
            self.userAge = Int(input) ?? self.userAge       // continue with default age (23) if no input is provided
        }))

    }
    func setupChartView(_ view: LineChartView) -> LineChartView{
        view.noDataText = "Loading..."
        view.noDataTextColor = .lightGray
        view.xAxis.drawGridLinesEnabled = false
        view.xAxis.drawAxisLineEnabled = false
        view.leftAxis.drawAxisLineEnabled = false
        view.xAxis.drawLabelsEnabled = false
        view.chartDescription?.enabled = false
        view.rightAxis.enabled = false
        view.drawBordersEnabled = false
        view.legend.enabled = false
        view.leftAxis.enabled = false
        return view
    }
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
    
        BleSingleton.shared.delegate = self

    }
    
    @IBAction func indexChanged(_ sender: UISegmentedControl) {
        let selection = sender.selectedSegmentIndex
        
        switch selection {
        case 0:
            temp_in_fah_ctrl_state = false
            break
        case 1:
            temp_in_fah_ctrl_state = true
            break
        default:
            break
        }
    }
    
    func updateChart(itemValue value: Double, _ chartCase: Int){
        if chartCase == 0{
            let chartValue = ChartDataEntry(x: Double(hrXvalue), y: value)
            hrXvalue+=1.0
            hrDataEntries.append(chartValue)
            let line = LineChartDataSet(entries: hrDataEntries)
            line.setColor(.gray)
            line.lineWidth = 1.5
            line.drawCirclesEnabled = false
            line.drawValuesEnabled = false
            let data = LineChartData()
            data.addDataSet(line)
            hrLineChartView.data = data
        }
//        else if chartCase == 1{
//            let chartValue = ChartDataEntry(x: Double(tempXvalue), y: Double(value))
//            tempXvalue+=1.0
//            tempDataEntries.append(chartValue)
//            let line = LineChartDataSet(entries: tempDataEntries)
//            line.setColor(.gray)
//            line.lineWidth = 1.5
//            line.drawCirclesEnabled = false
//            line.drawValuesEnabled = false
//            let data = LineChartData()
//            data.addDataSet(line)
//            tempLineChartView.data = data
//        }
        else if chartCase == 2 {
            let chartValue = ChartDataEntry(x: Double(respXvalue), y: Double(value))
            respXvalue+=1.0
            respDataEntries.append(chartValue)
            let line = LineChartDataSet(entries: respDataEntries)
            line.setColor(.gray)
            line.lineWidth = 1.5
            line.drawCirclesEnabled = false
            line.drawValuesEnabled = false
            let data = LineChartData()
            data.addDataSet(line)
            respLineChartView.data = data
        }
        else{
            return
        }
        
    }
    
    func hrDidChange(newVariableValue value: UInt8) {
        if value > 0 && value <= 200 {
            hr_field.text = String(value)
            updateChart(itemValue: Double(value), 0)
        
        }
        else{
            hr_field.text=""
        }
    }
    func tempDidChange(newVariableValue value: Double) {
        temp_disp = value
        if temp_in_fah_ctrl_state && !temp_in_fahrenheit!{
            // convert to Fahrenheit
            temp_disp = temp_disp! * 9.0 / 5.0 + 32.0
        }
        if !temp_in_fah_ctrl_state && temp_in_fahrenheit! {
            //convert to Celsius
            temp_disp = (temp_disp! - 32.0) * 5.0 / 9.0
        }
        if temp_disp! >= 0 && Int(hr_field.text ?? "") ?? 0 != 0{
            let text: String = String(format: "%.2f", temp_disp!) // change text in UI
//            print("temp: ", text)
            temp_field.text = String(text)
            
//            // chart will always have the celsius value
//            if temp_in_fah_ctrl_state{
//                let val = (temp_disp! - 32.0) * 5.0 / 9.0
//                updateChart(itemValue: val, 1)
//            }
//            else{
//                updateChart(itemValue: temp_disp!, 1)
//            }
        }
        else{
            temp_field.text = ""
        }
    }
    func tempInFarhrenheitDidChange(newVariableValue value: Bool) {        temp_in_fahrenheit = value
    }
    
    func respRateDidChange(newVariableValue value: UInt8) {
        if Int(hr_field.text ?? "") ?? 0 != 0 {
            resp_field.text = String(value)
            updateChart(itemValue: Double(value), 2)
        }
        else{
            resp_field.text = ""
        }
    }
    func curRMSSDDidChange(newVariableValue value: Float) {
        let percentage_stress = get_percentage_stress(Double(value), Int32(userAge))
        let actual_stress = 100 - percentage_stress
        stress_field.text = String(actual_stress)
    }

}
