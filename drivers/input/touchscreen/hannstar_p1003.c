/****************************************************************************************
 * driver/input/touchscreen/hannstar_p1003.c
 *Copyright 	:ROCKCHIP  Inc
 *Author	: 	sfm
 *Date		:  2010.2.5
 *This driver use for rk28 chip extern touchscreen. Use i2c IF ,the chip is Hannstar
 *description��
 ********************************************************************************************/
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <mach/board.h>

#define MAX_SUPPORT_POINT	2// //  4
//#define Singltouch_Mode
#define SAKURA_DBG                  0
#if SAKURA_DBG 
#define sakura_dbg_msg(fmt,...)       do {                                      \
                                   printk("sakura dbg msg------>"                       \
                                          " (func-->%s ; line-->%d) " fmt, __func__, __LINE__ , ##__VA_ARGS__); \
                                  } while(0)
#define sakura_dbg_report_key_msg(fmt,...)      do{                                                     \
                                                    printk("sakura report " fmt,##__VA_ARGS__);          \
                                                }while(0)
#else
#define sakura_dbg_msg(fmt,...)       do {} while(0)
#define sakura_dbg_report_key_msg(fmt,...)      do{}while(0)
#endif
struct point_data {	
	short status;	
	short x;	
	short y;
    short z;
};

struct multitouch_event{
	struct point_data point_data[MAX_SUPPORT_POINT];
	int contactid;
    int validtouch;
};

struct ts_p1003 {
	struct input_dev	*input;
	char			phys[32];
	struct delayed_work	work;

	struct i2c_client	*client;
    struct multitouch_event mt_event;
	u16			model;

	bool		pendown;
	bool 	 	status;
	int			irq;
	int 		has_relative_report;
	int			(*get_pendown_state)(void);
	void		(*clear_penirq)(void);
};

int p1003_get_pendown_state(void)
{
	return 0;
}

static void p1003_report_event(struct ts_p1003 *ts,struct multitouch_event *tc)
{
	struct input_dev *input = ts->input;
    int i,pandown = 0;
	dev_dbg(&ts->client->dev, "UP\n");
		
    for(i=0; i<MAX_SUPPORT_POINT;i++){			
        if(tc->point_data[i].status >= 0){
            pandown |= tc->point_data[i].status;
            input_report_abs(input, ABS_MT_TRACKING_ID, i);							
            input_report_abs(input, ABS_MT_TOUCH_MAJOR, tc->point_data[i].status);				
            input_report_abs(input, ABS_MT_WIDTH_MAJOR, 0);	
            input_report_abs(input, ABS_MT_POSITION_X, tc->point_data[i].x);				
            input_report_abs(input, ABS_MT_POSITION_Y, tc->point_data[i].y);				
            input_mt_sync(input);	

            sakura_dbg_report_key_msg("ABS_MT_TRACKING_ID = %x, ABS_MT_TOUCH_MAJOR = %x\n",
                " ABS_MT_POSITION_X = %x, ABS_MT_POSITION_Y = %x\n",i,tc->point_data[i].status,tc->point_data[i].x,tc->point_data[i].y);

            if(tc->point_data[i].status == 0)					
            	tc->point_data[i].status--;			
        }
        
    }	
    ts->pendown = pandown;
    input_sync(input);
}
static void p1003_report_single_event(struct ts_p1003 *ts,struct multitouch_event *tc)
{
	struct input_dev *input = ts->input;
    int cid;

    cid = tc->contactid;
    if (ts->status) {
        input_report_abs(input, ABS_X, tc->point_data[cid].x);
        input_report_abs(input, ABS_Y, tc->point_data[cid].y);
        input_sync(input);
    }
    if(ts->pendown != ts->status){
        ts->pendown = ts->status;
        input_report_key(input, BTN_TOUCH, ts->status);
        input_sync(input);
        sakura_dbg_report_key_msg("%s x =0x%x,y = 0x%x \n",ts->status?"down":"up",tc->point_data[cid].x,tc->point_data[cid].y);
    }
}
static inline int p1003_read_values(struct ts_p1003 *ts, struct multitouch_event *tc)
{
    s32 data;
    int len = 10;
    char buf[10];
    short contactid=0;

    data = i2c_master_normal_recv(ts->client, buf,len, 200*1000);

    if (data < 0 || (buf[0]!=0x04)) {
    	dev_err(&ts->client->dev, "i2c io error: %d or Hannstar read reg failed\n", data);
    	enable_irq(ts->irq);
    	return data;
    }

    contactid = (buf[1]&0x7C)>>2;
    tc->contactid = contactid;
    tc->point_data[contactid].status = buf[1]&0x01; 
    tc->point_data[contactid].x = ((buf[3]<<8) + buf[2])>>4;
    tc->point_data[contactid].y = ((buf[5]<<8) + buf[4])>>4;
    tc->validtouch = buf[1]&0x80;
    ts->status = tc->point_data[contactid].status;
//    printk("validtouch =%d,status= %d,contactid =%d\n",tc->validtouch,tc->point_data[contactid].status,contactid);
    return data;
}


static void p1003_work(struct work_struct *work)
{
	struct ts_p1003 *ts =
		container_of(to_delayed_work(work), struct ts_p1003, work);
	struct multitouch_event *tc = &ts->mt_event;
	u32 rt;
    
	rt = p1003_read_values(ts,tc);
    
    if(rt < 0)
        goto out;
#if defined (Singltouch_Mode)
    p1003_report_single_event(ts,tc);
#else
    p1003_report_event(ts,tc);
#endif
    
 out:
	if (ts->pendown)
		schedule_delayed_work(&ts->work,
				      msecs_to_jiffies(10));
	    
	else
		enable_irq(ts->irq);
}

static irqreturn_t p1003_irq(int irq, void *handle)
{
	struct ts_p1003 *ts = handle;
#if 1
	if (!ts->get_pendown_state || likely(ts->get_pendown_state())) {
		disable_irq_nosync(ts->irq);
		schedule_delayed_work(&ts->work,
				      msecs_to_jiffies(10));
	}

#endif
	return IRQ_HANDLED;
}

static void p1003_free_irq(struct ts_p1003 *ts)
{
	free_irq(ts->irq, ts);
	if (cancel_delayed_work_sync(&ts->work)) {
		/*
		 * Work was pending, therefore we need to enable
		 * IRQ here to balance the disable_irq() done in the
		 * interrupt handler.
		 */
		enable_irq(ts->irq);
	}
}

static int __devinit p1003_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct ts_p1003 *ts;
	struct p1003_platform_data *pdata = pdata = client->dev.platform_data;
	struct input_dev *input_dev;
	int err;

	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EIO;

	ts = kzalloc(sizeof(struct ts_p1003), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	ts->client = client;
	ts->irq = client->irq;
	ts->input = input_dev;
	ts->status =0 ;// fjp add by 2010-9-30
	ts->pendown = 0; // fjp add by 2010-10-06
	
	INIT_DELAYED_WORK(&ts->work, p1003_work);

	ts->model             = pdata->model;

	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "p1003 Touchscreen";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;

#if defined (Singltouch_Mode)
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_2, input_dev->keybit);
	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev,ABS_X,0,1087,0,0);
	input_set_abs_params(input_dev,ABS_Y,0,800,0,0);
#else
	ts->has_relative_report = 0;
	input_dev->evbit[0] = BIT_MASK(EV_ABS)|BIT_MASK(EV_KEY)|BIT_MASK(EV_SYN);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_dev->keybit[BIT_WORD(BTN_2)] = BIT_MASK(BTN_2); //jaocbchen for dual
	input_set_abs_params(input_dev, ABS_X, 0, 1087, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 800, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);
	input_set_abs_params(input_dev, ABS_HAT0X, 0, 1087, 0, 0);
	input_set_abs_params(input_dev, ABS_HAT0Y, 0, 800, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,0, 1087, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, 800, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);   
#endif

	if (pdata->init_platform_hw)
		pdata->init_platform_hw();

	if (!ts->irq) {
		dev_dbg(&ts->client->dev, "no IRQ?\n");
		return -ENODEV;
	}else{
		ts->irq = gpio_to_irq(ts->irq);
	}

	err = request_irq(ts->irq, p1003_irq, 0,
			client->dev.driver->name, ts);
	
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}
	
	if (err < 0)
		goto err_free_irq;
#if 0
	err = set_irq_type(ts->irq,IRQ_TYPE_LEVEL_LOW);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}
	if (err < 0)
		goto err_free_irq;
#endif
	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;

	i2c_set_clientdata(client, ts);

	return 0;

 err_free_irq:
	p1003_free_irq(ts);
	if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();
 err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
	return err;
}

static int __devexit p1003_remove(struct i2c_client *client)
{
	struct ts_p1003 *ts = i2c_get_clientdata(client);
	struct p1003_platform_data *pdata = client->dev.platform_data;

	p1003_free_irq(ts);

	if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();

	input_unregister_device(ts->input);
	kfree(ts);

	return 0;
}

static struct i2c_device_id p1003_idtable[] = {
	{ "p1003_touch", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, p1003_idtable);

static struct i2c_driver p1003_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "p1003_touch"
	},
	.id_table	= p1003_idtable,
	.probe		= p1003_probe,
	.remove		= __devexit_p(p1003_remove),
};

static int __init p1003_init(void)
{
	printk("--------> %s <-------------\n",__func__);
	return i2c_add_driver(&p1003_driver);
}

static void __exit p1003_exit(void)
{
	return i2c_del_driver(&p1003_driver);
}
module_init(p1003_init);
module_exit(p1003_exit);
MODULE_LICENSE("GPL");
