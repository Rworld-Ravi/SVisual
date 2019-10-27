/* eslint-disable no-unused-vars */

import React from "react"
import PropTypes from "prop-types";
import {Container, Row, Col, Button } from "react-bootstrap";
import AxisValue from "./axisValue.jsx"
import AxisTime from "./axisTime.jsx"
import Plot from "./plot.jsx"
import "../css/fontello.css";

export default
class Graph extends React.Component {

  constructor(props){
    super(props);   
     
    let axisParams = { valOffsPos : 0,
                       valDashStep : 100,  
                       tmOffsPos : 0,
                       tmDashStep : 100,
                       minValDashStep : 50,
                       maxValDashStep : 100}

    this.state = {tmInterval : { beginMs : Date.now(), endMs : Date.now() + 3.6e4}, 
                  valInterval : { begin : 0, end : 1000},
                  axisParams, 
                 };   

    this._signParams = {};

    this.handlePlotChange = this.handlePlotChange.bind(this); 
    this.handleAxisTimeChange = this.handleAxisTimeChange.bind(this);    
    this.handleAxisValueChange = this.handleAxisValueChange.bind(this);    
    
    this.handleAddSignal = this.handleAddSignal.bind(this); 
    this.handleDelSignal = this.handleDelSignal.bind(this);    

    this.handleResizeFull = this.handleResizeFull.bind(this);    
    this.handleResizeVertical = this.handleResizeVertical.bind(this);    
    this.handleResizeHorizontal = this.handleResizeHorizontal.bind(this);    
    this.handleChangeColor = this.handleChangeColor.bind(this);    
    this.handleAutoResize = this.handleAutoResize.bind(this);    
    this.handleClose = this.handleClose.bind(this);    
   
  }

  handleAxisTimeChange(tmInterval, axisParams){
    
    this.setState({tmInterval, axisParams });
  }

  handleAxisValueChange(valInterval, axisParams){
    
    this.setState({valInterval, axisParams });
  }

  handlePlotChange(tmInterval, valInterval, axisParams){
        
    this.setState({tmInterval, valInterval, axisParams});
  }

  handleAddSignal(name, module){

    this.props.onAddSignal(this.props.id, name, module);
  }

  handleDelSignal(name, module){
    
    delete this._signParams[name + module]
   
    this.props.onDelSignal(this.props.id, name, module);
  }

  handleResizeFull(){

    const signals = this.props.signals;

    let minValue = Number.MAX_VALUE, 
        maxValue = Number.MIN_VALUE;
    for (const k in signals){

      const sign = signals[k];

      for (const vals of sign.buffVals){
        
        for (const v of vals){
          if (v < minValue)
            minValue = v;
          if (v > maxValue)
            maxValue = v;
        }
      }
    }

    
  }

  handleResizeVertical(){

  }
  
  handleResizeHorizontal(){


  }

  handleChangeColor(){


  }

  handleAutoResize(){


  }

  handleClose(){


  }

  render(){
    
    let signals = this.props.signals,
        legend = [];
    for (let k in signals){

      if (!this._signParams[k]){ 
        this._signParams[k] = { lineWidth : 2,
                                transparent : 0.5,
                                color : '#'+Math.floor(Math.random()*16777215).toString(16) };
      }
      
      legend.push(
        <p key = {legend.length} 
           onClick = { () => this.handleDelSignal(signals[k].name, signals[k].module) } 
           style = {{ marginLeft : 10,
                      marginTop : 20 * legend.length,                                  
                      position : "absolute",                                               
                      color : this._signParams[k].color }}>
          {signals[k].name}
        </p>
      );
    }
    
    return (
      <Container-fluid >
        <Row noGutters={true} style={{ padding : "5px", backgroundColor : "grey"}}>
          <Col className="col-1"/>
          <Col className="col-10">
           <Button size="sm" className= { "icon-resize-full-alt"} style = {buttonStyle}
                   onClick = {this.handleResizeFull} />
           <Button size="sm" className= { "icon-resize-vertical"} style = {buttonStyle}
                   onClick = {this.handleResizeVertical} />
           <Button size="sm" className= { "icon-resize-horizontal"} style = {buttonStyle}
                   onClick = {this.handleResizeHorizontal} />
           <Button size="sm" className= { "icon-brush"} style = {buttonStyle}
                   onClick = {this.handleChangeColor} />
           <Button size="sm" className= { "icon-font"} style = {buttonStyle}
                   onClick = {this.handleAutoResize} />
          </Col>
          <Col className="col-1">
           <Button size="sm" className= { "icon-cancel"} style = {buttonStyle}
                   onClick = {this.handleClose} />
          </Col>
        </Row>
        <Row noGutters={true} style={{ paddingRight : "5px", backgroundColor : "grey"}}>
          <Col className="col-1">
            <AxisValue valInterval= { this.state.valInterval}
                       axisParams= { this.state.axisParams}
                       onChange = { this.handleAxisValueChange } />    
          </Col>
          <Col className="col-11">
            {legend}
            <Plot tmInterval= { this.state.tmInterval}
                  valInterval= { this.state.valInterval}
                  signals = { this.props.signals}
                  axisParams= { this.state.axisParams}
                  dataParams = {this.props.dataParams}
                  signParams = { this._signParams }
                  onChange = { this.handlePlotChange }
                  onDrop = { this.handleAddSignal } />            
          </Col>
        </Row>
        <Row noGutters={true} style={{ paddingRight : "5px", backgroundColor : "grey" }}>
          <Col className="col-1" >
          </Col>
          <Col>                
            <AxisTime tmInterval={ this.state.tmInterval}
                      axisParams={ this.state.axisParams}
                      onChange = { this.handleAxisTimeChange } /> 
          </Col>
        </Row>       
      </Container-fluid>
    )
  }
}

const buttonStyle = {
  marginLeft : "5px", 
  backgroundColor: "#747F74ff",
}

